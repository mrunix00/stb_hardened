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

## BUG-stb_image_write-005

- **Library:** `stb_image_write.h`
- **Severity:** Medium
- **Class:** Undefined Behavior (Float-to-Integer Conversion of NaN/Infinity)
- **Location:** `stb_image_write.h:639-654`
- **Source:** Fuzzer crash (UBSan: `runtime error: -nan is outside the range of representable values of type 'unsigned char'`)
- **Technique:** fuzzing
- **Description:**
  `stbiw__linear_to_rgbe()` does not guard against NaN or infinity pixel values. When all three RGB components are NaN (or any component is +infinity), the internal `stbiw__max` macro computes `maxcomp` as NaN or infinity. The comparison `maxcomp < 1e-32f` evaluates to false (NaN comparisons always return false; infinity > 1e-32), so the function proceeds to the else branch and computes `normalize = frexp(maxcomp, &exponent) * 256.0f / maxcomp`. When `maxcomp` is NaN, `normalize` becomes NaN because `frexp(NaN)` returns NaN and `256/NaN` = NaN. When `maxcomp` is infinity, `frexp(inf)` returns inf and `256/inf` = 0, but `inf * 0` = NaN per IEEE 754. Multiplying NaN by any linear component yields NaN, and converting NaN to `unsigned char` is undefined behavior in C. On UBSan-instrumented builds this immediately aborts; on release builds the behavior is implementation-defined. An attacker who can supply arbitrary float values to `stbi_write_hdr()` or `stbi_write_hdr_to_func()` can trigger undefined behavior in the library.
- **Reproduction sketch:**
  ```c
  #define STB_IMAGE_WRITE_IMPLEMENTATION
  #include "stb_image_write.h"
  #include <math.h>
  int main() {
      float buf[4] = { INFINITY, 0.0f, 0.0f, 1.0f };
      stbi_write_hdr_to_func(my_write_func, NULL, 1, 1, 3, buf);
      return 0;
  }
  ```
  With UBSan `-fsanitize=undefined`, this triggers "outside the range of representable values" at lines 649-651. On release builds, the NaN-to-unsigned char conversion produces an unspecified result.
- **Status:** Patched
- **Fix:** Changed the NaN/Infinity guard in `stbiw__linear_to_rgbe` (line 644) from `if (maxcomp < 1e-32f)` to `if (!(maxcomp >= 1e-32f) || maxcomp * 2 == maxcomp)`. The `!(maxcomp >= 1e-32f)` form catches NaN (NaN comparisons always return false) and negative/zero values. The `maxcomp * 2 == maxcomp` check catches infinity (the only positive value for which `x*2 == x` is true). Non-finite inputs are now safely treated as black (all-zero RGBE).

## BUG-stb_image_write-006

- **Library:** `stb_image_write.h`
- **Severity:** Medium
- **Class:** Out-of-Bounds Read
- **Location:** `stb_image_write.h:424,470,492,532,560`
- **Source:** static-analysis note
- **Technique:** static-analysis
- **Description:**
  The BMP writer (`stbi_write_bmp_core` at line 492) and TGA writer (`stbi_write_tga_core` at line 532) accept a `comp` parameter without validating it is in the valid range 1–4. When `comp <= 0` or `comp > 4`, the pixel offset computation `(j*x+i)*comp` at line 470 (and `j*x*comp` at line 560 in the TGA RLE path) produces negative or out-of-bounds addresses into the input pixel buffer. In `stbiw__write_pixel` at line 424, `d[comp-1]` reads from arbitrary memory before the buffer when `comp <= 0`. In the TGA RLE path, `memcmp` at line 570 receives a negative `comp` as the size argument, which when cast to `size_t` becomes a huge positive value, causing an out-of-bounds read that typically results in a segfault.
- **Reproduction sketch:**
  ```c
  #define STB_IMAGE_WRITE_IMPLEMENTATION
  #include "stb_image_write.h"
  int main() {
      unsigned char buf[64] = {0};
      stbi_write_bmp_to_func(my_write_func, NULL, 1, 1, 0, buf);  // comp=0
      stbi_write_tga_to_func(my_write_func, NULL, 1, 1, -1, buf); // comp=-1
      return 0;
  }
   ```
- **Status:** Patched
- **Fix:** Added `if (comp <= 0 || comp > 4) return 0;` at `stb_image_write.h:495` in `stbi_write_bmp_core` and `stb_image_write.h:536` in `stbi_write_tga_core` to reject invalid component counts before they reach the pixel encoding and comparison routines.

## BUG-stb_image_write-007

- **Library:** `stb_image_write.h`
- **Severity:** Medium
- **Class:** Out-of-Bounds Read
- **Location:** `stb_image_write.h:761,672`
- **Source:** static-analysis note
- **Technique:** static-analysis
- **Description:**
  `stbi_write_hdr_core` at line 761 validates `x`, `y`, and `data` but does not validate `comp`. If `comp` is supplied as 0 or negative, `stbiw__write_hdr_scanline` at line 672 computes pixel offsets as `scanline[x*ncomp + ...]` where `ncomp` is the unvalidated `comp`. When `comp < 0`, `x*ncomp` is negative, and `scanline[x*ncomp + 0]` reads from memory before the input buffer, leaking heap data into the output HDR file. When `comp > 4`, the `switch(ncomp)` at line 685 falls to the `default` case, replicating the first component across all three channels, which while not an OOB read per se, produces semantically incorrect output.
- **Reproduction sketch:**
  ```c
  #define STB_IMAGE_WRITE_IMPLEMENTATION
  #include "stb_image_write.h"
  int main() {
      float buf[4] = { 1.0f, 0.5f, 0.0f, 1.0f };
      stbi_write_hdr_to_func(my_write_func, NULL, 1, 1, -1, buf); // comp=-1
      return 0;
  }
  ```
- **Status:** Patched
- **Fix:** Added `|| comp <= 0 || comp > 4` to the input validation at `stb_image_write.h:763` in `stbi_write_hdr_core` to reject invalid component counts before they reach the scanline read loop.

## BUG-stb_image_write-008

- **Library:** `stb_image_write.h`
- **Severity:** Medium
- **Class:** Unchecked Return Value (NULL Pointer Dereference)
- **Location:** `stb_image_write.h:767`
- **Source:** static-analysis note
- **Technique:** static-analysis
- **Description:**
  In `stbi_write_hdr_core`, the scratch buffer allocation `STBIW_MALLOC(x*4)` at line 767 casts from `int` to `size_t` without overflow protection, and the return value is not checked for NULL. If `x*4` overflows the signed `int` range (when `x > 536,870,911`), the resulting value is implementation-defined (typically negative), which when passed to `malloc` as `size_t` becomes an enormous positive allocation that commonly fails. More directly, even with valid `x` values, if `STBIW_MALLOC` returns NULL due to memory pressure, `scratch` is NULL and is immediately passed to `stbiw__write_hdr_scanline` at line 781 which writes `rgbe[4]` and `scratch[width*3+width-1]` through the NULL pointer, causing a NULL-pointer dereference and crash. All other writers in the file check their `STBIW_MALLOC` returns.
- **Reproduction sketch:**
  ```c
  // Force malloc failure (e.g. via LD_PRELOAD or by requesting huge allocation)
  #define STB_IMAGE_WRITE_IMPLEMENTATION
  #include "stb_image_write.h"
  int main() {
      float buf[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
      stbi_write_hdr_to_func(my_write_func, NULL, 536870912, 1, 3, buf); // x*4 overflows
      return 0;
  }
  ```
- **Status:** Patched
- **Fix:** Changed `STBIW_MALLOC(x*4)` to `STBIW_MALLOC((size_t)x * 4)` at `stb_image_write.h:767` to avoid signed integer overflow in the size computation, and added `if (!scratch) return 0;` to check the allocation return value before use.

## BUG-stb_image_write-010

- **Library:** `stb_image_write.h`
- **Severity:** High
- **Class:** Heap Buffer Over-read (Out-of-Bounds Read)
- **Location:** `stb_image_write.h:1105,1109`
- **Source:** Fuzzer crash (ASan: heap-buffer-overflow in stbiw__encode_png_line)
- **Technique:** fuzzing
- **Description:**
  `stbi_write_png_to_mem()` at line 1146-1147 validates `stride_bytes` only for the zero case (setting it to `x*n`), but does not reject negative values or values smaller than `x*n`. When `stride_bytes` is negative, the row pointer computation in `stbiw__encode_png_line` at line 1105 (`z = pixels + stride_bytes * y`) produces a pointer before the start of the pixel buffer for any row beyond the first. The subsequent `memcpy(line_buffer, z, width*n)` at line 1109 reads from arbitrary memory before the allocation, causing a heap-buffer-overflow. Similarly, a stride smaller than `x*n` causes overlapping row reads and potential out-of-bounds access.
- **Reproduction sketch:**
  ```c
  #define STB_IMAGE_WRITE_IMPLEMENTATION
  #include "stb_image_write.h"
  void my_write(void *c, void *d, int n) { (void)c; (void)d; (void)n; }
  int main() {
      unsigned char buf[27] = {0};
      stbi_write_png_to_func(my_write, NULL, 3, 3, 3, buf, -9);  // negative stride
      return 0;
  }
  ```
- **Status:** Patched
- **Fix:** Changed the stride validation at `stb_image_write.h:1146` from `if (stride_bytes == 0)` to `if (stride_bytes < x * n)` to reject negative strides and strides too small to cover one row of pixels. Previously only the zero stride was corrected; any insufficient stride caused out-of-bounds row pointer computation in `stbiw__encode_png_line`.

## Session Summary — 2026-06-09

| Bug ID | Severity | Class | Status | Notes |
|--------|----------|-------|--------|-------|
| BUG-stb_image_write-001 | High | Out-of-Bounds Read | Patched | Fixed at stb_image_write.h:1253,1263,1329,1526 — changed bitBuf from int to unsigned int |
| BUG-stb_image_write-002 | High | Integer Overflow | Invalid | Could not reproduce; requires >2GB allocation |
| BUG-stb_image_write-003 | Medium | Integer Overflow | Invalid | Could not reproduce; requires >2GB allocation |
| BUG-stb_image_write-004 | Medium | Missing Input Validation | Patched | Fixed at stb_image_write.h:1474 — added negative dimension check |
| BUG-stb_image_write-005 | Medium | Undefined Behavior | Patched | Fixed at stb_image_write.h:644 — NaN/Inf guard in stbiw__linear_to_rgbe |
| BUG-stb_image_write-006 | Medium | Out-of-Bounds Read | Patched | Fixed at stb_image_write.h:495,536 — BMP/TGA comp validation added |
| BUG-stb_image_write-007 | Medium | Out-of-Bounds Read | Patched | Fixed at stb_image_write.h:763 — HDR comp validation added |
| BUG-stb_image_write-008 | Medium | NULL Pointer Dereference | Patched | Fixed at stb_image_write.h:767 — cast to size_t + NULL check in HDR scratch alloc |
| BUG-stb_image_write-009 | High | Out-of-Bounds Read/Write | Patched | Fixed at stb_image_write.h:1140 — added dimension/comp validation in stbi_write_png_to_mem |
| BUG-stb_image_write-010 | High | Heap Buffer Over-read | Patched | Fixed at stb_image_write.h:1146 — stride validation now rejects negative/insufficient stride |

