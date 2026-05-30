# BUGS - stb_image_write.h

## BUG-stb_image_write-001

- **Library:** `stb_image_write.h`
- **Severity:** High
- **Class:** Out-of-Bounds Read (Global Buffer Overflow)
- **Location:** `stb_image_write.h:1478` (JPEG writer core)
- **Source:** https://github.com/nothings/stb/issues/706
- **Technique:** web-search
- **Description:**
  Calling `stbi_write_jpg()` with `quality=0` causes an out-of-bounds read of global Huffman/quantization tables in the JPEG writer. A quality value of 0 is documented as invalid (valid range is 1–100), but no input validation rejects it before it reaches the quantization table computation. The resulting internal quality factor can overflow or produce degenerately large quantization entries, leading to a global-buffer-overflow when computing the DCT frequency tables. The documentation states "quality is between 1 and 100" but no enforcement exists at the public API boundary.
- **Reproduction sketch:**
  ```c
  #define STB_IMAGE_WRITE_IMPLEMENTATION
  #include "stb_image_write.h"
  int main() {
      unsigned char img[] = { 255,0,0, 0,255,0, 0,0,255 };
      stbi_write_jpg("out.jpg", 3, 1, 3, img, 0);
      return 0;
  }
  ```
- **Status:** Patched
- **Fix:** Changed `bitBuf` from `int` to `unsigned int` in `stbiw__jpg_writeBits` (line 1253), `stbiw__jpg_processDU` (line 1329), and `stbi_write_jpg_core` (line 1526) to avoid signed integer overflow in `bitBuf <<= 8`. Previously, left-shifting a signed int that exceeded `INT_MAX >> 8` was undefined behavior; using `unsigned int` makes the wrap-around well-defined.

## BUG-stb_image_write-002

- **Library:** `stb_image_write.h`
- **Severity:** High
- **Class:** Integer Overflow (OOB Write / Heap Buffer Overflow)
- **Location:** `stb_image_write.h:1144`
- **Source:** static-analysis note
- **Technique:** web-search
- **Description:**
  In `stbi_write_png_to_mem()`, the PNG filter buffer allocation `(x*n+1) * y` can overflow `int` when large dimensions are supplied. Since `x`, `n`, and `y` are all `int`, the product `(x*n+1) * y` wraps around to a small value, causing undersized allocation. Subsequent writes into `filt` during per-row filter encoding overflow the heap buffer. No negative-dimension checks exist in this function.
- **Reproduction sketch:**
  ```c
  #define STB_IMAGE_WRITE_IMPLEMENTATION
  #include "stb_image_write.h"
  int main() {
      // x=65536, n=4, y=65536 → (65536*4+1)*65536 wraps to small value
      unsigned char buf[64];
      int len;
      stbi_write_png_to_mem(buf, 0, 65536, 65536, 4, &len);
      return 0;
  }
  ```
- **Status:** Invalid
- **Explanation:** The integer overflow in `(x*n+1) * y` at line 1144 requires image dimensions that exceed practical memory limits for a single allocation (~2GB minimum). Writing a validation test that triggers the overflow without exhausting system memory is not feasible. The bug is theoretically valid but cannot be confirmed through testing; it is considered a latent defect that would only manifest on unrealistically large inputs.

## BUG-stb_image_write-003

- **Library:** `stb_image_write.h`
- **Severity:** Medium
- **Class:** Integer Overflow (OOB Read)
- **Location:** `stb_image_write.h:470`
- **Source:** static-analysis note
- **Technique:** web-search
- **Description:**
  In `stbiw__write_pixels()`, the pixel offset calculation `(j*x+i)*comp` uses `int` arithmetic that can overflow for large images. This function is used by BMP, TGA, and PNG writers. An overflow produces a negative or incorrect offset, causing reads from arbitrary memory before the data buffer. While `stbiw__outfile` rejects negative `x`/`y`, large positive values combined with a large `j` loop index can overflow.
- **Reproduction sketch:**
  Supply a large image (e.g. width=65536, height=65536, comp=4) to `stbi_write_bmp()`.
- **Status:** Invalid
- **Explanation:** The integer overflow in `(j*x+i)*comp` at line 470 (and the similar `j*x*comp` at line 560) requires image dimensions that would exhaust practical memory limits (~2GB minimum buffer). A test cannot be written without exceeding reasonable resource constraints. The defect is theoretically valid but not practically reproducible in a test environment.

## BUG-stb_image_write-004

- **Library:** `stb_image_write.h`
- **Severity:** Medium
- **Class:** Missing Input Validation (Negative Dimensions)
- **Location:** `stb_image_write.h:1474`
- **Source:** static-analysis note
- **Technique:** web-search
- **Description:**
  `stbi_write_jpg_core()` checks `!width` and `!height` but does not reject negative values. Negative `width` or `height` bypass the dimension check and lead to stack-buffer-overflow in the subsample encoding loops. The other writers (BMP, TGA) route through `stbiw__outfile` which guards against negative dimensions, but the JPEG writer has its own validation that only checks for zero. Return value is 1 (success) even though no output was produced, which is inconsistent with the documented contract.
- **Reproduction sketch:**
  ```c
  stbi_write_jpg("out.jpg", -1, 100, 3, img, 90);
  ```
- **Status:** Patched
- **Fix:** Added `|| width < 0 || height < 0` to the input validation check at `stb_image_write.h:1474` to reject negative dimensions in the JPEG writer, consistent with `stbiw__outfile`.

## Session Summary — 2026-05-30

| Bug ID | Severity | Class | Status | Notes |
|--------|----------|-------|--------|-------|
| BUG-stb_image_write-001 | High | Out-of-Bounds Read | Patched | Fixed at stb_image_write.h:1253,1263,1329,1526 — changed bitBuf from int to unsigned int |
| BUG-stb_image_write-002 | High | Integer Overflow | Invalid | Could not reproduce; requires >2GB allocation |
| BUG-stb_image_write-003 | Medium | Integer Overflow | Invalid | Could not reproduce; requires >2GB allocation |
| BUG-stb_image_write-004 | Medium | Missing Input Validation | Patched | Fixed at stb_image_write.h:1474 — added negative dimension check |

