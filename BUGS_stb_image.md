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

## Session Summary — 2026-05-30

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
