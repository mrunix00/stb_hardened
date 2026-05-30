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

## Session Summary — 2026-05-29

| Bug ID | Severity | Class | Status | Notes |
|--------|----------|-------|--------|-------|
| BUG-stb_image-001 | High | Integer Overflow | Patched | Fixed at stb_image.h:6994-7023 |
| BUG-stb_image-002 | Medium | Stack Overflow | Patched | Fixed at stb_image.h:6575, 6662-6667 |
| BUG-stb_image-003 | High | Integer Overflow | Invalid | Already protected by stbi__mad3sizes_valid |
| BUG-stb_image-004 | High | Integer Overflow | Invalid | Already protected by stbi__mad3sizes_valid |
