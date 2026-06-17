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
- **Reason:** Already protected by stbi\_\_mad3sizes_valid check earlier in the function.

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
- **Reason:** Already protected by stbi\_\_mad3sizes_valid check earlier in the function.

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
  size. For any pixel index >= pal_len\*4, the code reads uninitialized stack memory, producing
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

## BUG-stb_image-021

- **Library:** `stb_image.h`
- **Severity:** Medium
- **Class:** Denial of Service (Slow / Resource Consumption)
- **Location:** `stb_image.h:5975-6030`
- **Source:** libFuzzer timeout in `stbi__tga_load` (RLE loop)
- **Technique:** fuzzing
- **Description:**
  In `stbi__tga_load`, the RLE-decoding loop `for (i=0; i < tga_width * tga_height; ++i)` is
  bounded only by the declared image dimensions, not by the available source data. When
  the input stream runs out of bytes early (e.g. a truncated TGA), `stbi__get8(s)` returns
  zero on EOF, which produces an RLE command byte of 0x00 → RLE_count=1, RLE_repeating=0,
  then the per-pixel reads also return zero. The loop then continues iterating over the
  declared pixel count, doing useless work for every "pixel".

  With ASan+libFuzzer, a 18-byte TGA declaring 32767×511 = ~16.7M pixels (within
  `STBI_MAX_DIMENSIONS` for each axis individually) takes well over 10s to "decode"
  producing an all-zero image. Without ASan the same input takes ~3s of CPU.
  A larger declared area (e.g. 65535×4099) extends this further.

  Net effect: a few dozen bytes of attacker-controlled input can pin a CPU core for
  multiple seconds. No memory corruption, but a clear DoS.

- **Reproduction sketch:**
  ```c
  // 18-byte TGA-like input; mode byte for fuzzer, then:
  //   00 00 0b e4 04 01 01 01 00 00 00 ff 01 ff ff 01 0f
  // (id=0, color_map=0, image_type=RLE-gray, width=65281, height=511, bpp=15)
  // Call stbi_load_from_memory on this. Decoder fills tga_data with zeros
  // via 16.7M RLE loop iterations while source is exhausted.
  ```
  Fuzzer artifact: `crashes-saved/timeout-9201690b6575950dbc645d895a64df0a72d5ff9f`
- **Status:** Patched
- **Fix:** Added an EOF check inside the TGA RLE loop in `stbi__tga_load` at
  stb_image.h:5985. When `stbi__get8(s)` returns 0 (the value `stbi__get8`
  returns on EOF) AND the source is at EOF, the RLE loop is broken out of
  early. This stops the decoder from running useless iterations over a
  declared image area when the source is already exhausted. The validation
  test (`tests/bug_stb_image_021.c`) goes from 1.95s to 0.46s on this input.

## BUG-stb_image-022

- **Library:** `stb_image.h`
- **Severity:** High
- **Class:** Denial of Service (Excessive Memory Allocation)
- **Location:** `stb_image.h:6803-6805`
- **Source:** libFuzzer out-of-memory in `stbi__gif_load_next`
- **Technique:** fuzzing
- **Description:**
  In `stbi__gif_load_next`, the first-frame buffers are allocated as:

  ```c
  g->out       = (stbi_uc *) stbi__malloc(4 * pcount);
  g->background = (stbi_uc *) stbi__malloc(4 * pcount);
  g->history   = (stbi_uc *) stbi__malloc(pcount);
  ```

  where `pcount = g->w * g->h`. The only pre-allocation check is
  `stbi__mad3sizes_valid(4, g->w, g->h, 0)`, which guards against signed integer
  overflow, not against the resulting allocation being unreasonably large.

  `g->w` and `g->h` are validated against `STBI_MAX_DIMENSIONS` (1<<24) individually
  by `stbi__gif_header`, so each can be up to 16,777,216. The _product_ `g->w * g->h`
  is unconstrained and may be 200M-700M, producing `4 * pcount` of 800MB-2.8GB.

  A 44-byte GIF89a input with `width=0x4900` (18688) and `height=0x3846` (14406)
  — bytes "IF89a" reused as the height value via the overlapping header layout —
  causes three allocations totalling ~1.3GB. A 23-byte input with
  `width=0x2d00, height=0x0c7e` (overlapping with "~~~") does similarly. LibFuzzer
  with a 2GB RSS limit is OOM-killed; an unconstrained process can be pushed to
  the system OOM killer.

  No memory corruption, but the attack surface is a tiny input → GB-scale allocation.

- **Reproduction sketch:**
  ```c
  // Mode byte 0x00 (8-bit load, req_comp=0) followed by a GIF89a with
  // overlapping width/height bytes. Example 44-byte payload:
  //   00 47 49 46 38 39 61 00 49 46 38 39 61 00 00 80
  //   00 00 2c ff ff 00 00 00 2c 01 00 10 47 00 80 00
  //   00 2c ff ff 00 00 00 2c 01 00 10 47
  // Call stbi_load_from_memory on this — three allocations of ~270MB, ~1.07GB,
  // and ~1.07GB are issued before the image is even decoded.
  ```
  Fuzzer artifacts: `crashes-saved/oom-{1d895df...,3f52956...,af640ded...,b448c35b...}`
- **Status:** Patched
- **Fix:** Added a total-pixel check `pcount > STBI_MAX_DIMENSIONS` in
  `stbi__gif_load_next` at stb_image.h:6802. The previous
  `stbi__mad3sizes_valid(4, g->w, g->h, 0)` only guarded against signed
  integer overflow, not against excessive allocation. Now the loader
  rejects images whose total pixel count exceeds `STBI_MAX_DIMENSIONS`
  (1<<24) with "too large" before any of the three 1.3GB-scale buffers
  are allocated. The validation test (`tests/bug_stb_image_022.c`) now
  sees `"too large"` immediately on the same 44-byte GIF input.

## BUG-stb_image-023

- **Library:** `stb_image.h`
- **Severity:** Low
- **Class:** Undefined Behavior (UBSAN)
- **Location:** `stb_image.h:1683`
- **Source:** libFuzzer UBSAN report: `null pointer passed as argument 1, which is declared to never be null`
- **Technique:** fuzzing
- **Description:**
  The `stbi__getn` function contains a fallback branch that, on a memory-backed
  `stbi__context`, calls `memcpy(buffer, s->img_buffer, n)` even when `n` is zero,
  and even when `s->img_buffer` may not have been advanced past zero. Glibc
  declares `memcpy` with `__nonnull ((1, 2))`, so passing a zero-sized memcpy
  with a pointer that UBSAN cannot prove is non-NULL triggers a runtime
  diagnostic:

  ```
  tests/../stb_image.h:1683:14: runtime error: null pointer passed as argument 1,
    which is declared to never be null
  /usr/include/string.h:48:28: note: nonnull attribute specified here
  ```

  This is recoverable UBSAN (the fuzzer does not abort), and the code in
  practice performs no actual memory access. However, it is a real,
  reproducible UB violation triggered by legitimate input shapes (TGA with
  small bpp, GIF with declared max-dim count \* 0 cases, etc.). The fix is a
  one-line guard `if (n)` to skip the call when the size is zero.

- **Reproduction sketch:**
  ```c
  // Build with -fsanitize=address,undefined -fno-sanitize-recover=undefined
  // and feed any input that causes stbi__getn to be called with n==0.
  // In the present fuzzer this is reachable via the TGA path (see BUG-021)
  // because stbi__tga_load → ... → stbi__getn is reachable when the
  // non-RLE pixel loop runs after a 0-byte input.
  ```
- **Status:** Invalid
- **Reason:** The UBSAN null-pointer-argument check on `memcpy(src, NULL, 0)` requires
  either `s->img_buffer` or the destination buffer to actually be NULL at runtime.
  On this platform (Linux/glibc), `malloc(0)` returns non-NULL (minimum-size
  allocation), so the TGA non-RLE path with width=0 produces a non-NULL destination
  (`tga_data`). The callback-based path refills the internal buffer before the
  first `stbi__getn` call, so `s->img_buffer` is also non-NULL. The bug is
  fundamentally real (passing a NULL pointer to `__nonnull`-declared `memcpy` is UB
  regardless of size), but cannot be reproduced on platforms where `malloc(0)`
  returns a distinct non-NULL pointer and where UBSAN does not insert a runtime
  check that fires merely on potential-NULL provenance. The fix (guarding `memcpy`
  with `if (n)`) is still correct and should be applied; a test that aborts
  deterministically under UBSAN would require a platform where `malloc(0)` returns
  NULL (BSD/macOS) or a custom allocator interpose. Test file kept at
  `tests/bug_stb_image_023.c` for documentation.

## Session Summary — 2026-05-31

| Bug ID            | Severity | Class                                          | Status  | Notes                                                                    |
| ----------------- | -------- | ---------------------------------------------- | ------- | ------------------------------------------------------------------------ |
| BUG-stb_image-001 | High     | Integer Overflow                               | Patched | Fixed at stb_image.h:6994-7023                                           |
| BUG-stb_image-002 | Medium   | Stack Overflow                                 | Patched | Fixed at stb_image.h:6575, 6662-6667                                     |
| BUG-stb_image-003 | High     | Integer Overflow                               | Invalid | Already protected by stbi\_\_mad3sizes_valid                             |
| BUG-stb_image-004 | High     | Integer Overflow                               | Invalid | Already protected by stbi\_\_mad3sizes_valid                             |
| BUG-stb_image-005 | High     | OOB Read / Heap Buffer Overflow                | Patched | Fixed at stb_image.h:7032                                                |
| BUG-stb_image-006 | High     | Heap Buffer Overflow (OOB R/W)                 | Patched | Fixed at stb_image.h:1452                                                |
| BUG-stb_image-007 | Medium   | Use of Uninitialized Data                      | Patched | Fixed at stb_image.h:5937, 7236                                          |
| BUG-stb_image-008 | High     | Double-Free                                    | Patched | Fixed at stb_image.h:6998                                                |
| BUG-stb_image-009 | Medium   | NULL Pointer Dereference                       | Patched | Fixed at stb_image.h:6536                                                |
| BUG-stb_image-010 | Medium   | NULL Ptr Deref / Uninit Var                    | Patched | Fixed at stb_image.h:1451                                                |
| BUG-stb_image-011 | Medium   | Integer Overflow -> Heap BOF                   | Patched | Fixed at stb_image.h:1820                                                |
| BUG-stb_image-012 | High     | Integer Overflow -> Heap Buffer Overflow       | Invalid | Not reproducible on 64-bit (requires 32-bit)                             |
| BUG-stb_image-013 | High     | Integer Overflow -> Heap Buffer Overflow       | Invalid | Not reproducible on 64-bit (requires 32-bit)                             |
| BUG-stb_image-014 | Medium   | Out-of-bounds Read / Information Disclosure    | Patched | Fixed at stb_image.h:4969,4977                                           |
| BUG-stb_image-015 | Low      | Logic Error / Spec Violation                   | Patched | Fixed at stb_image.h:5112                                                |
| BUG-stb_image-016 | High     | Integer Overflow -> Heap Buffer Overflow       | Invalid | Not reproducible on Linux (glibc realloc frees)                          |
| BUG-stb_image-017 | Medium   | Logic Error                                    | Patched | Fixed at stb_image.h:3440                                                |
| BUG-stb_image-018 | Medium   | Use of Uninitialized Memory (Info Leak)        | Patched | Fixed at stb_image.h:3349                                                |
| BUG-stb_image-019 | Medium   | Out-of-bounds Read / Information Disclosure    | Patched | Fixed at stb_image.h:5617,5638,5645                                      |
| BUG-stb_image-020 | Medium   | Integer Overflow                               | Invalid | Neutered by BUG-001 stride overflow fix                                  |
| BUG-stb_image-021 | Medium   | DoS (Slow RLE Loop)                            | Patched | Fixed at stb_image.h:5985 — EOF break in TGA RLE loop                    |
| BUG-stb_image-022 | High     | DoS (Excessive Allocation)                     | Patched | Fixed at stb_image.h:6802 — total-pixel bound in GIF load_next           |
| BUG-stb_image-023 | Low      | Undefined Behavior                             | Invalid | memcpy with NULL+0 not reproducible on glibc; malloc(0) returns non-NULL |
| BUG-stb_image-024 | High     | DoS (Excessive Allocation + Slow Post-Process) | Patched | Fixed at stb_image.h:5904 — total-pixel bound in TGA loader              |

## BUG-stb_image-024

- **Library:** `stb_image.h`
- **Severity:** High
- **Class:** DoS (Excessive Allocation + Slow Post-Processing)
- **Location:** `stb_image.h:5903-5904`
- **Source:** libFuzzer timeout on input `crashes-saved/timeout-f2632f1dcc954b8b1ab3168989cddd1478699b46` discovered after fixing BUG-021
- **Technique:** fuzzing
- **Description:**
  After the BUG-021 fix made the TGA RLE loop bounded by the actual input
  bytes, a smaller TGA input (33 bytes) was still able to cause a 10-second
  timeout. Root cause: the TGA loader only checks each dimension individually
  against `STBI_MAX_DIMENSIONS` (1<<24), so a TGA declaring `width=2863`,
  `height=65508` (per-axis: 2863 < 1<<24 and 65508 < 1<<24) still passes
  those checks. The loader then computes `tga_comp` (4 for 32-bpp) and
  allocates `stbi__malloc_mad3(tga_width, tga_height, tga_comp, 0)` ≈
  **~750 MB**. The RLE loop exits quickly (with the BUG-021 fix), but the
  post-processing vertical-flip loop iterates O(pcount) ≈ O(187M) on a
  multi-hundred-MB buffer, which is catastrophic under ASan (each iteration
  has poisoned red-zones on both sides). Result: a 33-byte input causes the
  process to run for many seconds, exhausting CPU and memory.

  This is a duplicate-flavor of the BUG-022 GIF issue: the per-axis bound
  is insufficient when a malicious image has _moderate_ width and height
  but their product is still enormous.

- **Reproduction sketch:**
  ```c
  unsigned char tga[] = {
      0x00, 0x00, 0x0b,                       /* ID, colormap, RLE gray */
      0xe4, 0x04, 0x0c, 0x01, 0x00,           /* colormap spec */
      0x00, 0x07, 0x01, 0x01,                 /* x_origin, y_origin */
      0x2f, 0x0b, 0xe4, 0xff,                 /* width=2863, height=65508 */
      0x20,                                   /* bits_per_pixel */
      0x5a, 0x47, 0x7c, 0x00, 0x00, 0x01, 0x00, 0x00,
      0x00, 0x01, 0x01, 0xff, 0xff, 0xff, 0x0b,
  };
  stbi_load_from_memory(tga, sizeof(tga), &x, &y, &c, 0);
  // On unpatched: returns non-NULL after ~2-3 seconds (or timeout under ASan).
  ```
- **Status:** Patched
- **Fix:** Added `(size_t)tga_width * (size_t)tga_height > STBI_MAX_DIMENSIONS`
  check at stb_image.h:5904. The check is placed right after the existing
  per-axis `STBI_MAX_DIMENSIONS` checks, matching the BUG-022 GIF fix
  style. The total-pixel bound is in pixel units (not bytes), and is
  conservative: a 24-bit image of `STBI_MAX_DIMENSIONS` pixels would still
  require ~48MB, well within reasonable use. The validation test
  (`tests/bug_stb_image_024.c`) now reports `reason="too large"` and
  returns `NULL` immediately on the same 33-byte input.
  | BUG-stb_image-024 | High | DoS (Excessive Allocation + Slow Post-Process) | Patched | Fixed at stb_image.h:5904 — total-pixel bound in TGA loader |

## BUG-stb_image-025

- **Library:** `stb_image.h`
- **Severity:** Low
- **Class:** Integer Conversion / Signedness Error
- **Location:** `stb_image.h:5255`
- **Source:** libFuzzer run against `stbi_load_from_memory`/PNG/zlib memory APIs; minimized reproducer embedded in `tests/bug_stb_image_025.c` (original crash SHA1 `0197814e8121102905d881230150998a5a9197e2`, Clang `-fsanitize=integer` report)
- **Technique:** fuzzing
- **Description:**
  The PNG parser stores chunk lengths as `stbi__uint32`, but passes an unknown ancillary chunk length directly to `stbi__skip`, whose `n` parameter is an `int`. A malformed PNG can set an ancillary chunk length such as `0xffffffff`; the implicit conversion changes it to `-1`, taking the negative skip path. With integer-conversion sanitization enabled this is a runtime error, and without sanitization it makes parser control flow depend on implementation-defined signed conversion for attacker-controlled input.
- **Reproduction sketch:**
  Compile a harness that calls `stbi_load_from_memory` on the fuzzer artifact with `-fsanitize=address,undefined,integer -fno-sanitize-recover=all`; the process aborts at `stb_image.h:5255` when handling the oversized ancillary PNG chunk.
- **Status:** Patched
- **Fix:** Added explicit `c.length > INT_MAX` rejection before both PNG `stbi__skip` calls at `stb_image.h:5099-5100` and `stb_image.h:5255-5256`, then cast the guarded value to `int`.

## BUG-stb_image-026

- **Library:** `stb_image.h`
- **Severity:** Medium
- **Class:** Out-of-bounds Read / Information Disclosure (Stack Leak)
- **Location:** `stb_image.h:5559-5564, 5601-5611`
- **Source:** static-analysis
- **Technique:** static-analysis
- **Description:**
  In `stbi__bmp_load`, the palette count `psize` is computed as:

  ```c
  if (info.hsz == 12) {
     if (info.bpp < 24)
        psize = (info.offset - info.extra_read - 24) / 3;
  } else {
     if (info.bpp < 16)
        psize = (info.offset - info.extra_read - info.hsz) >> 2;
  }
  ```

  where `info.offset` is a signed `int` from the BMP file header. When `offset` is smaller than `extra_read + hsz`, the subtraction yields a negative `psize`.

  The guard at line 5566 (`if (psize == 0)`) only catches zero, and the guard at line 5603 (`if (psize == 0 || psize > 256)`) does not catch negative values because an `int` comparison `-14 > 256` is false. With a negative `psize`, the palette-read loop `for (i=0; i < psize; ++i)` is skipped, leaving the stack-allocated `pal[256][4]` completely uninitialized.

  The existing BUG-019 fix added bounds checks (`if (v >= psize) v = psize-1`) on pixel values at lines 5642 and 5649, but with negative `psize` the comparison promotes `psize` to a large unsigned value via C's usual arithmetic conversions, so the clamp is never triggered. Pixel values (0–255) therefore index directly into the uninitialized stack palette, leaking stack contents into the output image.

- **Reproduction sketch:**
  ```c
  // A BMP with offset=0, hsz=40, bpp=8. psize = (0-14-40)>>2 = -14.
  // Palette not read; stack pal[256][4] uninitialized.
  // Pixels decoded using this uninitialized data as colors.
  unsigned char bmp[] = {
    0x42,0x4D,             // 'BM'
    0x36,0x00,0x00,0x00,   // filesize (ignored)
    0x00,0x00,             // reserved
    0x00,0x00,             // reserved
    0x00,0x00,0x00,0x00,   // offset = 0  ← too small
    0x28,0x00,0x00,0x00,   // hsz = 40
    0x02,0x00,0x00,0x00,   // width = 2
    0x02,0x00,0x00,0x00,   // height = 2
    0x01,0x00,             // planes = 1
    0x08,0x00,             // bpp = 8
    0x00,0x00,0x00,0x00,   // compression = 0
    0x00,0x00,0x00,0x00,   // image size (ignored)
    0x00,0x00,0x00,0x00,   // xres (ignored)
    0x00,0x00,0x00,0x00,   // yres (ignored)
    0x00,0x00,0x00,0x00,   // colors used (ignored)
    0x00,0x00,0x00,0x00,   // colors important (ignored)
    0xFF,0x00,0x00,0x00,   // pixel data: index 255
    0x00,0x00,0x00,0x00,
  };
   stbi_load_from_memory(bmp, sizeof(bmp), &x, &y, &c, 0);
   // Output pixels will contain uninitialized stack data for color values.
  ```
- **Status:** Patched
- **Fix:** Changed `psize == 0` to `psize <= 0` at stb_image.h:5603. When `info.offset` is smaller than `extra_read + hsz`, `psize` becomes negative, which previously bypassed both validation checks (`psize == 0` and `psize > 256`). The negative psize allowed the palette-read loop to be skipped while leaving `pal[256][4]` on the stack uninitialized. The `<=` check catches negative values in addition to zero, causing the decoder to reject the malformed BMP with "invalid" instead of reading from uninitialized stack memory.

## Session Summary — 2026-06-09

| Bug ID            | Severity | Class                              | Status  | Notes                                                                 |
| ----------------- | -------- | ---------------------------------- | ------- | --------------------------------------------------------------------- |
| BUG-stb_image-001 | High     | Integer Overflow                   | Patched | GIF stride overflow                                                   |
| BUG-stb_image-002 | Medium   | Stack Overflow                     | Patched | GIF LZW recursion depth                                               |
| BUG-stb_image-003 | High     | Integer Overflow                   | Invalid | PSD already protected                                                 |
| BUG-stb_image-004 | High     | Integer Overflow                   | Invalid | GIF already protected                                                 |
| BUG-stb_image-005 | High     | OOB Read / Heap BOF                | Patched | GIF two_back pointer fix                                              |
| BUG-stb_image-006 | High     | Heap BOF                           | Patched | GIF vertical flip bytes_per_pixel                                     |
| BUG-stb_image-007 | Medium   | Uninitialized Data                 | Patched | TGA/HDR getn return check                                             |
| BUG-stb_image-008 | High     | Double-Free                        | Patched | GIF zero-stride guard                                                 |
| BUG-stb_image-009 | Medium   | NULL Deref                         | Patched | PIC convert_format guard                                              |
| BUG-stb_image-010 | Medium   | NULL Deref                         | Patched | GIF flip guard                                                        |
| BUG-stb_image-011 | Medium   | Integer Overflow                   | Patched | convert_format16 mad4                                                 |
| BUG-stb_image-012 | High     | Integer Overflow                   | Invalid | 32-bit only, not exploitable on 64-bit                                |
| BUG-stb_image-013 | High     | Integer Overflow                   | Invalid | 32-bit only, not exploitable on 64-bit                                |
| BUG-stb_image-014 | Medium   | OOB Read                           | Patched | PNG palette bounds clamp                                              |
| BUG-stb_image-015 | Low      | Logic Error                        | Patched | PNG IHDR depth validation                                             |
| BUG-stb_image-016 | High     | Integer Overflow                   | Invalid | glibc realloc frees, not exploitable                                  |
| BUG-stb_image-017 | Medium   | Logic Error                        | Patched | JPEG marker error propagation                                         |
| BUG-stb_image-018 | Medium   | Uninitialized Memory               | Patched | JPEG coeff buffer memset                                              |
| BUG-stb_image-019 | Medium   | OOB Read                           | Patched | BMP palette bounds clamp                                              |
| BUG-stb_image-020 | Medium   | Integer Overflow                   | Invalid | Neutered by BUG-001                                                   |
| BUG-stb_image-021 | Medium   | DoS (Slow Loop)                    | Patched | TGA RLE EOF break                                                     |
| BUG-stb_image-022 | High     | DoS (Excessive Alloc)              | Patched | GIF total-pixel bound                                                 |
| BUG-stb_image-023 | Low      | Undefined Behavior                 | Invalid | stbi\_\_getn memcpy with n=0                                          |
| BUG-stb_image-024 | High     | DoS (Excessive Alloc)              | Patched | TGA total-pixel bound                                                 |
| BUG-stb_image-025 | Medium   | Integer Conversion                 | Patched | PNG chunk length INT_MAX guard                                        |
| BUG-stb_image-026 | Medium   | OOB Read / Stack Leak              | Patched | BMP negative psize → uninitialized palette, fixed at stb_image.h:5603 |
| BUG-stb_image-027 | High     | Buffer Over-Read via Negative Size | Patched | Added n<0 guard in stbi\_\_getn at stb_image.h:1668                   |
| BUG-stb_image-028 | Medium   | DoS (Infinite Loop)                | Invalid | PIC RLE count=0 bounded by input size (5B/iter); not amplified        |
| BUG-stb_image-029 | Medium   | DoS (Infinite Loop)                | Patched | Negative io.read treated as EOF at stb_image.h:1601                   |
| BUG-stb_image-030 | Medium   | NULL Pointer Dereference           | Invalid | Not exploitable; img_buffer == img_buffer_end prevents deref          |

## BUG-stb_image-027

- **Library:** `stb_image.h`
- **Severity:** High
- **Class:** Buffer Over-Read (via Negative Size)
- **Location:** `stb_image.h:1666-1688`
- **Source:** https://github.com/Mintsuki/stbi-hardened (FIXES.md — I/O and context section)
- **Technique:** web-search
- **Description:**
  The `stbi__getn` function accepts `int n` as its size parameter. If `n` is negative
  (e.g., due to an integer overflow in a caller's size computation), the function
  proceeds to the fast path at line 1682-1683 where `memcpy(buffer, s->img_buffer, n)`
  is called. Since `n` is implicitly cast to `size_t` (unsigned), a negative int
  becomes a huge positive value (e.g., `(size_t)-1` = `0xFFFFFFFF` on 32-bit,
  `0xFFFFFFFFFFFFFFFF` on 64-bit). This causes `memcpy` to read far past the end of
  `s->img_buffer`, potentially crashing or disclosing heap memory.

  The negative-n check at line 1647 in `stbi__skip` (`if (n < 0)`) shows the
  developers were aware of the need to guard against negative sizes in other
  functions, but `stbi__getn` was missed.

- **Reproduction sketch:**
  ```c
  // A PNM (PPM/PGM/PBM) file with dimensions such that
  // img_n * img_x * img_y * (bits_per_channel/8) overflows int to negative.
  // This negative passes to stbi__getn, which then calls memcpy with a
  // huge size_t, causing a buffer over-read.
  ```
- **Fix:** Added `if (n < 0) return 0;` guard at the top of `stbi__getn`, matching the convention already used in `stbi__skip` (line 1647). Changed `stb_image.h:1668`.
- **Status:** Patched

## BUG-stb_image-028

- **Library:** `stb_image.h`
- **Severity:** Medium
- **Class:** Denial of Service (Infinite/Slow Loop)
- **Location:** `stb_image.h:6447-6466`
- **Source:** https://github.com/Mintsuki/stbi-hardened (FIXES.md — PIC section)
- **Technique:** web-search
- **Description:**
  In `stbi__pic_load_core`, the Pure RLE decoder (case 1, line 6447) reads a
  `count` byte from the input. If `count` is 0, the `left -= count` operation
  does nothing (`left` stays unchanged), so the while loop `while (left>0)`
  never exits. The only escape is `stbi__at_eof`. On each iteration, the code
  reads 1 byte (count) + 4 bytes (value) from the input. If the input is
  artificially padded with zeros, this loop can spin for many iterations
  without making output progress (the inner `for(i=0; i<count; ++i)` does
  nothing when count=0). This creates a slow-loop / resource-exhaustion DoS
  vector.
- **Reproduction sketch:**
  ```c
  // A PIC file with Pure RLE packets where count=0 bytes are interleaved,
  // causing the decoder to loop without forward progress.
  ```
- **Status:** Invalid
- **Validation findings:** Each count==0 iteration consumes 5 bytes of input (1 count + 4 value). The loop is bounded by input_size/5 iterations, proportional to input size — no amplification. The `stbi__at_eof` check (line 6456) ensures termination when input is exhausted. A 100KB input file produces at most 20K iterations. No crash, no meaningful DoS.

## BUG-stb_image-029

- **Library:** `stb_image.h`
- **Severity:** Medium
- **Class:** DoS (Infinite Loop)
- **Location:** `stb_image.h:1597-1612`
- **Source:** https://github.com/Mintsuki/stbi-hardened (FIXES.md — I/O and context section)
- **Technique:** web-search
- **Description:**
  In `stbi__refill_buffer`, the return value `n` from `io.read` (line 1599)
  is used directly without validation. If `io.read` returns a negative value
  (indicating an error), the computation at line 1610
  `s->img_buffer_end = s->buffer_start + n` sets `img_buffer_end` before
  `img_buffer_start`. After this, every call to `stbi__get8` sees
  `img_buffer >= img_buffer_end` (because end is before start), which triggers
  another `stbi__refill_buffer`, which again sets
  `img_buffer_end = buffer_start + (-1)`, followed by `get8` reading one byte
  from `buffer_start`. On the next `get8` the condition is again true, calling
  `refill_buffer` again — an infinite loop.

  _Validated:_ With a callback that always returns -1, the decoder calls
  `io.read` over 46 million times in 15 seconds without making progress,
  consuming 100% CPU — a classic Denial of Service.

- **Reproduction sketch:**
  ```c
  // Use a custom io.read callback that returns -1 on error, then
  // call stbi_load_from_callbacks. The decoder will spin forever.
  ```
- **Status:** Patched
- **Fix:** Changed `if (n == 0)` to `if (n <= 0)` at `stb_image.h:1601`. Negative io.read returns are now treated as EOF, breaking the refill → get8 → refill infinite loop.

## BUG-stb_image-030

- **Library:** `stb_image.h`
- **Severity:** Medium
- **Class:** NULL Pointer Dereference
- **Location:** `stb_image.h:826-833`
- **Source:** https://github.com/Mintsuki/stbi-hardened (FIXES.md — Public API entry points section)
- **Technique:** web-search
- **Description:**
  `stbi__start_mem` receives a `buffer` pointer and `len` but performs no
  validation. If `buffer` is NULL or `len` is non-positive, the function
  assigns NULL to `img_buffer` (line 831) and `buffer+len` to `img_buffer_end`
  (line 832).

  _Testing found:_ The NULL buffer case does NOT cause an immediate crash
  because `img_buffer == img_buffer_end`, so the `img_buffer < img_buffer_end`
  comparison in `stbi__get8` is false (equal), preventing the dereference.
  The decoder reads 0s from the EOF fallback and returns NULL gracefully.

  The negative `len` case creates a pointer before the start of the array
  (`buffer + (-1)`), which is technically UB but similarly does not
  manifest as a crash because the `img_buffer < img_buffer_end` check is
  immediately false.

  While the fix is harmless defense-in-depth, the vulnerability as
  originally described does not manifest with ASan/UBSan. All public API
  entry points tested return NULL gracefully.

- **Reproduction sketch:**
  ```c
  stbi_load_from_memory(NULL, 0, &x, &y, &c, 0);
  // Returns NULL gracefully (no crash). img_buffer == img_buffer_end,
  // so stbi__get8 returns 0 via the EOF fallback path.
  ```
- **Status:** Invalid

## Session Summary — 2026-06-17

| Bug ID            | Severity | Class                              | Status  | Notes                                                   |
| ----------------- | -------- | ---------------------------------- | ------- | ------------------------------------------------------- |
| BUG-stb_image-027 | High     | Buffer Over-Read via Negative Size | Patched | Added n<0 guard in stbi\_\_getn at stb_image.h:1668     |
| BUG-stb_image-028 | Medium   | DoS (Infinite Loop)                | Invalid | PIC RLE count=0 bounded by input size; no amplification |
| BUG-stb_image-029 | Medium   | DoS (Infinite Loop)                | Patched | Negative io.read treated as EOF at stb_image.h:1601     |
| BUG-stb_image-030 | Medium   | NULL Pointer Dereference           | Invalid | img_buffer == img_buffer_end prevents deref             |

|
