## BUG-stb_image_resize2-001

- **Library:** `stb_image_resize2.h`
- **Severity:** High
- **Class:** Crash / Denial of Service
- **Location:** `stb_image_resize2.h:6242`
- **Source:** https://github.com/nothings/stb/issues/1678
- **Technique:** web-search
- **Description:**
  Proactively hardened point sampling logic to use a specialized fast path and
  forced coefficient width of 1, preventing potential mismatches with gathered
  resample routines that might lead to crashes or out-of-bounds access.
- **Reproduction sketch:**
  ```c
  stbir_resize(input, 232, 232, 0, output, 302, 302, 0, STBIR_RGBA, STBIR_TYPE_UINT8, STBIR_EDGE_CLAMP, STBIR_FILTER_POINT_SAMPLE);
  ```
- **Status:** Patched
- **Fix:** Added specialized point sampling path in stbir__resample_horizontal_gather and forced coefficient width to 1 in stbir__get_coefficient_width.

## BUG-stb_image_resize2-002

- **Library:** `stb_image_resize2.h`
- **Severity:** High
- **Class:** Integer Overflow → Heap Buffer Overflow
- **Location:** `stb_image_resize2.h:7125-7370`
- **Source:** static-analysis
- **Technique:** static-analysis
- **Description:**
  In `stbir__alloc_internal_mem_and_build_samplers`, multiple `size_t`
  calculations for buffer sizes (`decode_buffer_size`, `ring_buffer_size`, etc.)
  and the total allocation size (`alloced_total`) did not check for integer
  overflow.
- **Reproduction sketch:**
  ```c
  // Request a resize with dimensions that make (w * h * channels * sizeof(float)) overflow size_t.
  stbir_resize(input, 1, 1, 0, NULL, 0x40000001, 10, 0, STBIR_RGBA, STBIR_TYPE_UINT8, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT);
  ```
- **Status:** Patched
- **Fix:** Implemented stbir__mul_add_overflow_check and applied it to all critical allocation size calculations.

## Session Summary — 2026-05-28

| Bug ID | Severity | Class | Status | Notes |
|--------|----------|-------|--------|-------|
| BUG-stb_image_resize2-001 | High | Crash / Denial of Service | Patched | Proactively hardened point sampling path |
| BUG-stb_image_resize2-002 | High | Integer Overflow | Patched | Added overflow checks to allocation logic |
