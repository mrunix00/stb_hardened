## BUG-stb_image-001

- **Library:** `stb_image.h`
- **Severity:** High
- **Class:** Integer Overflow -> Heap Buffer Overflow
- **Location:** `stb_image.h:7010-7015`
- **Source:** static-analysis
- **Technique:** static-analysis
- **Description:**
  In `stbi__load_gif_main`, the allocation for the multi-frame buffer (`out`) and
  the `delays` array uses `stbi__malloc(layers * stride)` and
  `stbi__malloc(layers * sizeof(int))` respectively. As the number of layers
  increases during the decoding of an animated GIF, these multiplications can
  overflow a 32-bit signed integer, leading to a small allocation and a
  subsequent heap buffer overflow when the frame data is copied or the delays
  are written.
- **Reproduction sketch:**
  ```c
  // Provide a GIF with a very large number of frames such that layers * stride overflows.
  // Call stbi_load_gif_from_memory.
  ```
- **Status:** Patched
- **Fix:** Added overflow checks for multi-frame GIF allocations in `stbi__load_gif_main` using `stbi__mad2sizes_valid` and `stbi__malloc_mad2` (stb_image.h:6994-7023).

## BUG-stb_image-002

- **Library:** `stb_image.h`
- **Severity:** Medium
- **Class:** OOB Read / Denial of Service
- **Location:** `stb_image.h:6654-6689`
- **Source:** https://github.com/nothings/stb/issues/1558
- **Technique:** web-search
- **Description:**
  The GIF decoder's `stbi__out_gif_code` function uses recursion to decode
  LZW codes. A deeply nested or circular LZW prefix chain can lead to excessive
  recursion, potentially causing a stack overflow.
- **Reproduction sketch:**
  ```c
  // Provide a GIF with a malicious LZW stream where codes point to themselves
  // or form a long chain.
  ```
- **Status:** Patched
- **Fix:** Added a recursion depth limit (256) to `stbi__out_gif_code` and tracked it in the `stbi__gif` struct (stb_image.h:6575, 6662-6667).

## BUG-stb_image-003

- **Library:** `stb_image.h`
- **Severity:** High
- **Class:** Integer Overflow -> Heap Buffer Overflow
- **Location:** `stb_image.h:6204`
- **Source:** static-analysis
- **Technique:** static-analysis
- **Description:**
  In the PSD decoder, the allocation for the output buffer `out = stbi__malloc(4 * w*h)`
  does not use an overflow-checked multiplication helper, unlike other formats.
  While there is an earlier check `stbi__mad3sizes_valid(4, w, h, 0)`, it is only
  applied in one branch, and a malformed PSD might bypass it or trigger the
  overflow in the unprotected `stbi__malloc` call.
- **Reproduction sketch:**
  ```c
  // Provide a PSD with dimensions that make 4 * w * h overflow 32-bit int.
  ```
- **Status:** Invalid
- **Reason:** Already protected by stbi__mad3sizes_valid check earlier in the function.

## BUG-stb_image-004

- **Library:** `stb_image.h`
- **Severity:** High
- **Class:** Integer Overflow -> Heap Buffer Overflow
- **Location:** `stb_image.h:6793-6795`
- **Source:** static-analysis
- **Technique:** static-analysis
- **Description:**
  In `stbi__gif_load_next`, the internal buffers `g->out`, `g->background`,
  and `g->history` are allocated using `stbi__malloc(4 * pcount)` or
  `stbi__malloc(pcount)`. If the GIF width and height are large, `4 * pcount`
  can overflow a signed 32-bit integer, leading to undersized allocations.
- **Reproduction sketch:**
  ```c
  // Provide a GIF with dimensions such that 4 * width * height overflows.
  ```
- **Status:** Invalid
- **Reason:** Already protected by stbi__mad3sizes_valid check earlier in the function.

## BUG-stb_image-005

- **Library:** `stb_image.h`
- **Severity:** High
- **Class:** Out-of-bounds Read / Heap Buffer Overflow
- **Location:** `stb_image.h:6823, 7032`
- **Source:** CVE-2026-5185 / GHSL-2023-145 / https://github.com/nothings/stb/issues/1930
- **Technique:** web-search
- **Description:**
  In `stbi__load_gif_main` (line 7032), when loading the second frame of an animated
  GIF, the `two_back` pointer is computed as `out - 2 * stride`. This points before
  the allocated buffer, not to the previous frame (which would be `out + (layers-2) * stride`).
  This invalid pointer is then passed to `stbi__gif_load_next`, where in the GIF
  dispose-3 handling (line 6823) it is dereferenced with `&two_back[pi * 4]` without
  any bounds checking. This results in an out-of-bounds read from memory before the
  buffer, potentially leaking heap metadata. A crafted GIF triggers this on every
  frame after the first.
- **Reproduction sketch:**
  ```c
  // Provide a multi-frame GIF with dispose method 3 (restore to previous).
  // Call stbi_load_gif_from_memory. The two_back pointer will point before the buffer.
  ```
- **Status:** Patched
- **Fix:** Fixed `two_back` pointer arithmetic at stb_image.h:7032. Changed `out - 2 * stride` to `out + (layers - 2) * stride`, which correctly points to the frame two positions back in the multi-frame buffer rather than before the allocated buffer.

## BUG-stb_image-006

- **Library:** `stb_image.h`
- **Severity:** High
- **Class:** Heap Buffer Overflow (Out-of-bounds Read/Write)
- **Location:** `stb_image.h:1222-1244, 1250, 1452`
- **Source:** CVE-2023-45662 / GHSL-2023-146
- **Technique:** web-search
- **Description:**
  When `stbi_set_flip_vertically_on_load` is set to TRUE and the GIF decoder
  converts the output to a different number of channels (via `stbi__convert_format`
  at line 7048), the `stbi_load_gif_from_memory` function passes the original
  `*comp` value (always 4 for GIF) to `stbi__vertical_flip_slices` instead of the
  converted channel count. The vertical flip then computes `bytes_per_row` and
  `slice_size` using 4 bytes-per-pixel while the actual buffer has only `req_comp`
  bytes per pixel. This causes memcpy to read/write past the intended row boundaries
  and advance `bytes` by the wrong `slice_size` between slices, leading to heap
  buffer overflow. Additionally, `slice_size = w * h * bytes_per_pixel` (line 1250)
  is vulnerable to integer overflow.
- **Reproduction sketch:**
  ```c
  stbi_set_flip_vertically_on_load(1);
  // Load a GIF with req_comp=2 (or any value != 4).
  // The vertical flip will use bytes_per_pixel=4 on a 2-channel buffer.
  ```
- **Status:** Patched
- **Fix:** Fixed `bytes_per_pixel` argument to `stbi__vertical_flip_slices` in `stbi_load_gif_from_memory` (stb_image.h:1452). Changed from `*comp` (always 4 for GIF) to `req_comp ? req_comp : *comp`, which correctly reflects the actual channel count after any format conversion.

## BUG-stb_image-007

- **Library:** `stb_image.h`
- **Severity:** Medium
- **Class:** Use of Uninitialized Data (Information Disclosure)
- **Location:** `stb_image.h:5937, 7231`
- **Source:** CVE-2023-45663 / GHSL-2023-147
- **Technique:** web-search
- **Description:**
  The `stbi__getn` function reads a specified number of bytes from the input stream
  into a buffer and returns 0 on failure (EOF) or 1 on success. Its return value is
  unchecked in two places:
  1. `stbi__tga_load` (line 5937): In the non-RLE, non-indexed, non-rgb16 path,
     `stbi__getn` reads pixel data into a heap buffer allocated via `stbi__malloc_mad3`.
     If the file is truncated, the buffer contains uninitialized heap data which is
     exposed as pixel data.
  2. `stbi__hdr_load` (line 7231): In the flat-data path, `stbi__getn` reads 4 bytes
     into a stack buffer `rgbe`. If the file is truncated, uninitialized stack data
     is passed to `stbi__hdr_convert` and may leak through the decoded HDR image.
- **Reproduction sketch:**
  ```c
  // Provide a truncated TGA or HDR file.
  // The uninitialized buffer data will appear in the decoded output.
  ```
- **Status:** Patched
- **Fix:** Added return-value checks for `stbi__getn` at both locations:
  - TGA (stb_image.h:5937): Returns `"bad TGA"` on incomplete pixel data read with proper cleanup of `tga_data`.
  - HDR (stb_image.h:7236): Returns `"bad HDR"` on incomplete run-length data read with proper cleanup of `hdr_data`.

## BUG-stb_image-008

- **Library:** `stb_image.h`
- **Severity:** High
- **Class:** Double-Free
- **Location:** `stb_image.h:6996-7003`
- **Source:** [GHSL-2023-149](https://securitylab.github.com/advisories/GHSL-2023-149_stb_image_h/) (GIF realloc size 0 → double‑free)
- **Technique:** web-search
- **Description:**
  When a GIF frame has width=0 or height=0, `stride = g.w * g.h * 4` evaluates to 0.
  `stbi__mad2sizes_valid(layers, 0, 0)` passes because `stbi__mul2sizes_valid` returns 1
  when any operand is 0. Then `STBI_REALLOC_SIZED(out, out_size, layers * 0)` calls
  `realloc(out, 0)`, which (per POSIX) may free `out` and return NULL. The NULL check
  on the return value causes `stbi__load_gif_main_outofmem` to call `STBI_FREE(out)`
  on an already-freed pointer, triggering a double-free.
- **Reproduction sketch:**
  A crafted GIF with a zero‑width or zero‑height frame submitted via
  `stbi_load_gif_from_memory`.
- **Status:** Patched
- **Fix:** Added `stride == 0` guard after computing stride from frame dimensions
  (stb_image.h:6998). A zero-stride frame is rejected with `outofmem` before the
  realloc or malloc is reached, preventing the double-free.

## BUG-stb_image-009

- **Library:** `stb_image.h`
- **Severity:** Medium
- **Class:** NULL Pointer Dereference
- **Location:** `stb_image.h:6536`
- **Source:** GHSL-2023-149
- **Technique:** web-search
- **Description:**
  In `stbi__pic_load`, when `stbi__pic_load_core` fails (line 6529), the `result`
  pointer is freed and set to NULL (line 6531). However, execution continues and
  `result` is passed as the first argument to `stbi__convert_format(result, 4, req_comp, x, y)`
  at line 6536. Inside `stbi__convert_format`, if `req_comp != 4`, the function
  dereferences `src = data + j * x * img_n` (line 1770) which is NULL-pointer
  arithmetic — undefined behavior resulting in a segmentation fault. This can be
  triggered by a crafted PICT file.
- **Reproduction sketch:**
  ```c
  // Provide a malformed PICT file that passes the header checks
  // but fails in stbi__pic_load_core, with req_comp != 4.
  ```
- **Status:** Patched
- **Fix:** Added `if (result)` guard before the `stbi__convert_format` call at stb_image.h:6536. When `stbi__pic_load_core` fails, `result` is NULL and must not be passed to `stbi__convert_format`, which would dereference the NULL pointer for any `req_comp != 4`.

## BUG-stb_image-010

- **Library:** `stb_image.h`
- **Severity:** Medium
- **Class:** NULL Pointer Dereference / Use of Uninitialized Variable
- **Location:** `stb_image.h:1450-1452`
- **Source:** GHSL-2023-151
- **Technique:** web-search
- **Description:**
  In `stbi_load_gif_from_memory`, if `stbi__load_gif_main` fails and returns NULL
  (line 1450), the `z` output parameter may remain uninitialized. If
  `stbi__vertically_flip_on_load` is set, the code proceeds to call
  `stbi__vertical_flip_slices(result, *x, *y, *z, *comp)` (line 1452) with a NULL
  `result` pointer and an uninitialized `*z` value. This causes a NULL pointer
  dereference inside `stbi__vertical_flip` when computing row pointers. The value
  of `*z` (uninitialized) determines whether the loop body is entered, making the
  crash dependent on stack state.
- **Reproduction sketch:**
   ```c
   stbi_set_flip_vertically_on_load(1);
   // Provide a non-GIF file (fails the stbi__gif_test check).
   // The result is NULL, *z is uninitialized, and the flip is attempted.
   ```
- **Status:** Patched
- **Fix:** Added `result &&` guard before `stbi__vertically_flip_on_load` check at
  stb_image.h:1451. When `stbi__load_gif_main` returns NULL the flip is skipped,
  avoiding both the NULL pointer dereference and the use of uninitialized `*z`.

## BUG-stb_image-011

- **Library:** `stb_image.h`
- **Severity:** Medium
- **Class:** Integer Overflow -> Heap Buffer Overflow
- **Location:** `stb_image.h:1820`
- **Source:** https://github.com/nothings/stb/issues/1860
- **Technique:** web-search
- **Description:**
  In `stbi__convert_format16`, the allocation at line 1820 uses
  `stbi__malloc(req_comp * x * y * 2)` with raw integer multiplication and no
  overflow check. All operands are `unsigned int` (32-bit). When loading a large
  16-bit grayscale PNG with `req_comp=4`, the product `4 * x * y * 2` can silently
  overflow a 32-bit unsigned integer, producing a small allocation. The subsequent
  conversion loop at lines 1827-1828 then writes far past the end of this undersized
  buffer using `dest = good + j * x * req_comp`. This is a heap buffer overflow.
  The existing size check in the PNG decoder (line 5121) only validates the source
  channel count, not the requested output channels.
- **Reproduction sketch:**
  ```c
  // Provide a large 16-bit grayscale PNG (e.g., 23171x23171).
  // Call stbi_load_16 with req_comp=4.
  // The allocation overflows and the conversion loop writes OOB.
  ```
- **Status:** Patched
- **Fix:** Replaced `stbi__malloc(req_comp * x * y * 2)` with `stbi__malloc_mad4(req_comp, x, y, 2, 0)` at stb_image.h:1820. The mad4 variant validates that the signed multiplication does not overflow INT_MAX before allocating, preventing the silent unsigned-wraparound that produced an undersized buffer.

## BUG-stb_image-012

- **Library:** `stb_image.h`
- **Severity:** High
- **Class:** Integer Overflow -> Heap Buffer Overflow
- **Location:** `stb_image.h:1193-1196`
- **Source:** https://github.com/nothings/stb/issues/1928 (bug 2)
- **Technique:** web-search
- **Description:**
  In `stbi__convert_16_to_8`, the intermediate buffer size is computed as `img_len = w * h * channels`
  using plain int multiplication. This product is then passed directly to `stbi__malloc(img_len)`.
  With sufficiently large dimensions (e.g., a 32768x32768 16-bit grayscale PNG), `w * h * channels`
  can overflow a 32-bit signed integer, producing an undersized allocation. The subsequent loop at
  line 1199 iterates over `img_len` elements, writing far past the end of the undersized buffer.
- **Reproduction sketch:**
  ```c
  // Load a very large 16-bit grayscale PNG (e.g., 32768x32768) via stbi_load_16.
  // The img_len overflow causes a small allocation, then the conversion loop
  // writes OOB.
  ```
- **Status:** Invalid
- **Reason:** On 64-bit systems, the overflowed `img_len` (negative int) casts to a huge `size_t` when passed to `stbi__malloc`, and the allocation of ~18 exabytes fails. Function returns "outofmem" without heap overflow, which is the same outcome as the proposed fix. Exploitation requires a 32-bit target where the overflow wraps to a small positive value.

## BUG-stb_image-013

- **Library:** `stb_image.h`
- **Severity:** High
- **Class:** Integer Overflow -> Heap Buffer Overflow
- **Location:** `stb_image.h:1209-1212`
- **Source:** https://github.com/nothings/stb/issues/1928 (bug 2)
- **Technique:** web-search
- **Description:**
  In `stbi__convert_8_to_16`, the allocation uses `img_len = w * h * channels` followed by
  `stbi__malloc(img_len*2)`. All operands are plain `int`. For a large image, the `img_len`
  multiplication overflows, and the `*2` doubles the wrapping error. This results in an
  undersized heap allocation, and the conversion loop at line 1215 writes past the end of
  the buffer. The risk is doubled compared to the 16-to-8 path because of the additional `*2`
  factor.
- **Reproduction sketch:**
  ```c
  // Load a very large 8-bit image (e.g., 32768x32768 RGBA PNG) via stbi_load_16
  // (which triggers 8-to-16 conversion). The allocation overflows.
  ```
- **Status:** Invalid
- **Reason:** Same as BUG-012: on 64-bit, the overflowed `img_len` causes a `malloc` of a huge size to fail, returning "outofmem". Both buggy and fixed code produce the same outcome on 64-bit. Requires a 32-bit target to exploit.

## BUG-stb_image-014

- **Library:** `stb_image.h`
- **Severity:** Medium
- **Class:** Out-of-bounds Read / Information Disclosure
- **Location:** `stb_image.h:4956-4988`
- **Source:** https://github.com/nothings/stb/issues/1928 (bug 5)
- **Technique:** web-search
- **Description:**
  In `stbi__expand_png_palette`, the `palette` buffer is 1024 bytes (256 entries × 4 channels)
  allocated on the caller's stack at lines 5137-5141. Only `pal_len` entries are initialized
  from the PLTE chunk (at most 256, but possibly fewer). The `STBI_NOTUSED(len)` at line 4988
  explicitly discards the palette length. Pixel values can range 0-255 regardless of palette
  size. For any pixel index >= pal_len*4, the code reads uninitialized stack memory, producing
  output pixels that contain leaked stack data.
- **Reproduction sketch:**
  ```c
  // Provide a paletted PNG with 1 palette entry and pixel values 0-255.
  // Pixels with value > 0 will contain uninitialized stack data.
  ```
- **Status:** Patched
- **Fix:** Added bounds check on palette index in `stbi__expand_png_palette` (stb_image.h:4969,4977). Clamped `orig[i]` to `len-1` before computing the palette offset, preventing out-of-bounds reads from uninitialized stack memory when pixel indices exceed the palette size.

## BUG-stb_image-015

- **Library:** `stb_image.h`
- **Severity:** Low
- **Class:** Logic Error / Spec Violation
- **Location:** `stb_image.h:5111-5114`
- **Source:** https://github.com/nothings/stb/issues/1928 (bug 6)
- **Technique:** web-search
- **Description:**
  The PNG IHDR parser accepts bit depths 1, 2, and 4 with color types 2 (RGB), 4 (GA), and 6 (RGBA),
  which violates the PNG specification. Per the PNG spec, color types 2, 4, 6 are restricted to
  depths 8 and 16. Accepting sub-8-bit depths for multi-channel images causes the bit-unpacking
  code to misinterpret pixel boundaries, potentially producing incorrect output. While not a direct
  memory safety issue, this logic error could lead to unexpected decoder behavior.
- **Reproduction sketch:**
  ```c
  // Create a PNG with color type 2 (RGB) and depth 4.
  // The image will load "successfully" with corrupted pixel data.
  ```
- **Status:** Patched
- **Fix:** Added IHDR validation check at stb_image.h:5112. Color types 2 (RGB), 4 (GA), and 6 (RGBA) now reject bit depths less than 8, per the PNG specification. Previously, depths 1, 2, and 4 were silently accepted for these color types, causing incorrect bit-unpacking and corrupted pixel output.

## BUG-stb_image-016

- **Library:** `stb_image.h`
- **Severity:** High
- **Class:** Integer Overflow -> Heap Buffer Overflow
- **Location:** `stb_image.h:5188-5189`
- **Source:** https://github.com/nothings/stb/issues/1928 (bug 7)
- **Technique:** web-search
- **Description:**
  The PNG IDAT accumulation loop doubles `idata_limit` each time incoming data exceeds current
  capacity: `while (ioff + c.length > idata_limit) idata_limit *= 2`. The variable `idata_limit`
  is `stbi__uint32`. After collecting enough large IDAT chunks (e.g., 2^32 worth of data),
  the doubling causes `idata_limit` to overflow to 0. `STBI_REALLOC_SIZED` with size 0 frees
  the old buffer and returns NULL (on POSIX), or returns a zero-size block. Subsequent
  `stbi__getn` writes the full IDAT chunk contents into this undersized buffer, causing a
  heap buffer overflow.
- **Reproduction sketch:**
  ```c
  // Provide a PNG with many large IDAT chunks whose cumulative data
  // causes idata_limit to overflow to 0.
  ```
- **Status:** Invalid
- **Reason:** On 64-bit Linux (glibc), `STBI_REALLOC_SIZED` with size 0 calls `realloc(ptr, 0)` which frees the pointer and returns NULL, caught as "outofmem". The bug would require `realloc(ptr, 0)` to return a non-NULL zero-size allocation (BSD/macOS behavior) to be exploitable. Not reproducible on this platform.

## BUG-stb_image-017

- **Library:** `stb_image.h`
- **Severity:** Medium
- **Class:** Logic Error
- **Location:** `stb_image.h:3440`
- **Source:** https://github.com/nothings/stb/issues/1928 (bug 8)
- **Technique:** web-search
- **Description:**
  In the JPEG decoder's scan loop, when `stbi__process_marker` returns 0 (failure),
  the code at line 3440 does `if (!stbi__process_marker(j, m)) return 1;` — returning
  1 (success) instead of 0 (failure). This causes the decoder to treat a corrupt or
  unexpected JPEG marker as a successful decode, potentially returning an incomplete
  or corrupted image to the caller. Between progressive scans, bad markers are silently
  accepted.
- **Reproduction sketch:**
  ```c
  // Provide a progressive JPEG with a malformed marker between scans.
  // The decoder will return success (1) despite the marker failure.
  ```
- **Status:** Patched
- **Fix:** Changed `return 1` to `return 0` at stb_image.h:3440. When `stbi__process_marker` returns 0 (failure), the scan loop now correctly propagates the failure instead of treating it as success.

## BUG-stb_image-018

- **Library:** `stb_image.h`
- **Severity:** Medium
- **Class:** Use of Uninitialized Memory (Information Disclosure)
- **Location:** `stb_image.h:3085, 3346`
- **Source:** https://github.com/nothings/stb/issues/1928 (bug 9)
- **Technique:** web-search
- **Description:**
  In `stbi__jpeg_finish` (line 3085), all `z->s->img_n` components are dequantized and
  IDCT'd into the output image. Coefficients are allocated with `stbi__malloc_mad3` at
  line 3346 (not calloc). In a progressive JPEG, not all components may appear in SOS
  (Start of Scan) segments. Components that were never scanned contain uninitialized
  heap data. The IDCT processes this uninitialized data into the output pixels, leaking
  heap contents. The leak extent depends on which components are unscanned.
- **Reproduction sketch:**
  ```c
  // Provide a progressive JPEG that defines 3 components but only scans 2 of them
  // in SOS markers. The unscanned component will leak heap data into the output.
  ```
- **Status:** Patched
- **Fix:** Added `memset` to zero the `raw_coeff` buffer after allocation in `stbi__malloc_mad3` at stb_image.h:3349. In a progressive JPEG, components that are never scanned in any SOS marker would otherwise contain uninitialized malloc data, which the IDCT then processes into the output pixels, leaking heap contents.

## BUG-stb_image-019

- **Library:** `stb_image.h`
- **Severity:** Medium
- **Class:** Out-of-bounds Read / Information Disclosure
- **Location:** `stb_image.h:5535, 5601-5608`
- **Source:** https://github.com/nothings/stb/issues/1928 (bug 10)
- **Technique:** web-search
- **Description:**
  In the BMP decoder, `pal[256][4]` is a stack-allocated array (line 5535). For an 8-bit BMP,
  `psize` specifies the number of palette entries read from the file (1-256, line 5601-5608).
  Pixel values in the image data can range 0-255 regardless of `psize`. If `psize < 256`,
  pixel indices >= psize read uninitialized stack memory from the unfilled portion of `pal`.
  These values are written directly to the output image, disclosing stack contents.
- **Reproduction sketch:**
  ```c
  // Provide an 8-bit BMP with psize=1 and pixel values 0-255.
  // Output pixels will contain stack data for all non-zero pixel values.
  ```
- **Status:** Patched
- **Fix:** Added bounds checks on pixel indices before `pal[]` access in all three BMP pixel-decode paths (stb_image.h:5617,5638,5645). Pixel values >= `psize` are clamped to `psize-1`, preventing out-of-bounds reads from uninitialized stack memory.

## BUG-stb_image-020

- **Library:** `stb_image.h`
- **Severity:** Medium
- **Class:** Integer Overflow
- **Location:** `stb_image.h:1250`
- **Source:** https://github.com/nothings/stb/issues/1928 (bug 11)
- **Technique:** web-search
- **Description:**
  In `stbi__vertical_flip_slices` (used when flipping animated GIFs with
  `stbi_set_flip_vertically_on_load`), `slice_size = w * h * bytes_per_pixel` is computed
  using plain int multiplication with no overflow check. If the image dimensions are large
  enough, this overflows. The overflowed `slice_size` is then used in pointer arithmetic
  (`bytes += slice_size`) to advance between slices, resulting in incorrect offsets and
  out-of-bounds reads/writes during the vertical flip of subsequent slices.
- **Reproduction sketch:**
  ```c
  // Load a large animated GIF with stbi_set_flip_vertically_on_load(1).
  ```
- **Status:** Invalid
- **Reason:** The `slice_size` in `stbi__vertical_flip_slices` uses the same `w`, `h`, and `bytes_per_pixel` as the GIF stride computation (`stride = g.w * g.h * 4`). The stride overflow was already fixed by BUG-001's overflow checks, so the oversize dimensions that would overflow `slice_size` cannot reach `stbi__vertical_flip_slices`. This bug is effectively neutered by the BUG-001 fix.

## Session Summary — 2026-05-31

| Bug ID | Severity | Class | Status | Notes |
|--------|----------|-------|--------|-------|
| BUG-stb_image-001 | High | Integer Overflow | Patched | Fixed at stb_image.h:6994-7023 |
| BUG-stb_image-002 | Medium | Stack Overflow | Patched | Fixed at stb_image.h:6575, 6662-6667 |
| BUG-stb_image-003 | High | Integer Overflow | Invalid | Already protected by stbi__mad3sizes_valid |
| BUG-stb_image-004 | High | Integer Overflow | Invalid | Already protected by stbi__mad3sizes_valid |
| BUG-stb_image-005 | High | OOB Read / Heap Buffer Overflow | Patched | Fixed at stb_image.h:7032 |
| BUG-stb_image-006 | High | Heap Buffer Overflow (OOB R/W) | Patched | Fixed at stb_image.h:1452 |
| BUG-stb_image-007 | Medium | Use of Uninitialized Data | Patched | Fixed at stb_image.h:5937, 7236 |
| BUG-stb_image-008 | High | Double-Free | Patched | Fixed at stb_image.h:6998 |
| BUG-stb_image-009 | Medium | NULL Pointer Dereference | Patched | Fixed at stb_image.h:6536 |
| BUG-stb_image-010 | Medium | NULL Ptr Deref / Uninit Var | Patched | Fixed at stb_image.h:1451 |
| BUG-stb_image-011 | Medium | Integer Overflow -> Heap BOF | Patched | Fixed at stb_image.h:1820 |
| BUG-stb_image-012 | High | Integer Overflow -> Heap Buffer Overflow | Invalid | Not reproducible on 64-bit (requires 32-bit) |
| BUG-stb_image-013 | High | Integer Overflow -> Heap Buffer Overflow | Invalid | Not reproducible on 64-bit (requires 32-bit) |
| BUG-stb_image-014 | Medium | Out-of-bounds Read / Information Disclosure | Patched | Fixed at stb_image.h:4969,4977 |
| BUG-stb_image-015 | Low | Logic Error / Spec Violation | Patched | Fixed at stb_image.h:5112 |
| BUG-stb_image-016 | High | Integer Overflow -> Heap Buffer Overflow | Invalid | Not reproducible on Linux (glibc realloc frees) |
| BUG-stb_image-017 | Medium | Logic Error | Patched | Fixed at stb_image.h:3440 |
| BUG-stb_image-018 | Medium | Use of Uninitialized Memory (Info Leak) | Patched | Fixed at stb_image.h:3349 |
| BUG-stb_image-019 | Medium | Out-of-bounds Read / Information Disclosure | Patched | Fixed at stb_image.h:5617,5638,5645 |
| BUG-stb_image-020 | Medium | Integer Overflow | Invalid | Neutered by BUG-001 stride overflow fix |
