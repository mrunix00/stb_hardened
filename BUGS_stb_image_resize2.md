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

## BUG-stb_image_resize2-003

- **Library:** `stb_image_resize2.h`
- **Severity:** High
- **Class:** Undefined Behavior (Signed Integer Overflow)
- **Location:** `stb_image_resize2.h:8121-8123`
- **Source:** static-analysis
- **Technique:** static-analysis
- **Description:**
  In `stbir_quick_resize_helper`, when `output_stride_in_bytes == INT_MIN`,
  the negation `-positive_output_stride_in_bytes` at line 8123 triggers
  signed integer overflow (undefined behavior). Two's-complement negation of
  INT_MIN is INT_MAX+1, which cannot be represented in `int`.
  Although the subsequent bounds check at line 8126
  (`positive_output_stride_in_bytes < scanline_output_in_bytes`) would
  catch the wrapped value in practice, the UB allows a compiler to optimize
  away the check entirely or produce arbitrary code.
- **Reproduction sketch:**
  ```c
  // output_stride_in_bytes = INT_MIN (-2147483648)
  int output_stride = -2147483648;
  stbir_resize(input, 100, 100, 0, output, 100, 100, output_stride,
               STBIR_RGBA, STBIR_TYPE_UINT8, STBIR_EDGE_CLAMP,
               STBIR_FILTER_DEFAULT);
  ```
- **Status:** Patched
- **Validation test:** `tests/bug_stb_image_resize2_003.c` — UBSan reports negation of INT_MIN cannot be represented in type `int`.
- **Fix:** Added guard for `INT_MIN` (= `0x80000000u`) before negation at line 8133, returning 0 immediately.

## BUG-stb_image_resize2-004

- **Library:** `stb_image_resize2.h`
- **Severity:** Critical
- **Class:** Out-of-Bounds Function Pointer Call
- **Location:** `stb_image_resize2.h:3113-3127`
- **Source:** static-analysis
- **Technique:** static-analysis
- **Description:**
  The `stbir__edge_wrap_slow[]` array at line 3113 has exactly 4 entries
  (indices 0-3 for CLAMP, REFLECT, WRAP, ZERO). The `stbir__edge_wrap()`
  inline function at line 3126 calls:
  ```c
  return stbir__edge_wrap_slow[edge]( n, max );
  ```
  without bounds-checking `edge`. If `edge > 3`, the read is out of bounds
  and the value found at that memory location is used as a function pointer.
  An attacker who can control the `stbir_edge` value (e.g. through
  `stbir_set_edgemodes`) can redirect execution to an arbitrary address.
- **Reproduction sketch:**
  ```c
  STBIR_RESIZE resize;
  stbir_set_edgemodes(&resize, (stbir_edge)42, (stbir_edge)42);
  stbir_build_samplers(&resize);
  ```
- **Status:** Patched
- **Validation test:** `tests/bug_stb_image_resize2_004.c` — triggers `stbir__edge_wrap_slow[42]` OOB; crashes at `stbir__get_extents` assertion before returning from `stbir_resize_extended`
- **Fix:** Added edge enum validation in `stbir__perform_build` at lines 7948-7949. Returns 0 immediately when `horizontal_edge` or `vertical_edge` exceed `STBIR_EDGE_ZERO` (3), preventing any OOB access to `stbir__edge_wrap_slow[]`.

## BUG-stb_image_resize2-005

- **Library:** `stb_image_resize2.h`
- **Severity:** High
- **Class:** Missing Input Validation
- **Location:** `stb_image_resize2.h:7840`
- **Source:** static-analysis
- **Technique:** static-analysis
- **Description:**
  `stbir_set_edgemodes()` stores `horizontal_edge` and `vertical_edge`
  directly into the `resize` struct without validating they fall within the
  valid `stbir_edge` range (0-3). A caller (or attacker) may pass values
  > 3, which eventually reach `stbir__edge_wrap_slow[edge]` in
  `stbir__perform_build` / `stbir__set_sampler` / `stbir__edge_wrap`,
  producing an OOB function-pointer call.
- **Reproduction sketch:**
  ```c
  stbir_set_edgemodes(&resize, (stbir_edge)7, (stbir_edge)7);
  stbir_build_samplers(&resize);
  ```
- **Status:** Patched
- **Validation test:** `tests/bug_stb_image_resize2_005.c` — `stbir_set_edgemodes` returns 1 for invalid edge values, confirming missing validation.
- **Fix:** Added bounds checks at the top of `stbir_set_edgemodes`: `if ((unsigned)horizontal_edge > STBIR_EDGE_ZERO) return 0;` (same for vertical). This prevents invalid edge values from being stored in the resize struct at the API boundary.

## BUG-stb_image_resize2-006

- **Library:** `stb_image_resize2.h`
- **Severity:** High
- **Class:** Missing Input Validation → Out-of-Bounds Array Index
- **Location:** `stb_image_resize2.h:7853-7858`, `stb_image_resize2.h:7070-7074`
- **Source:** static-analysis
- **Technique:** static-analysis
- **Description:**
  `stbir_set_pixel_layouts()` stores `input_pixel_layout` and
  `output_pixel_layout` without validating they are within the valid range
  (0-16). The lookup array `stbir__pixel_layout_convert_public_to_internal[]`
  at line 7070 has exactly 17 entries (indices 0-16). A value > 16 produces
  an OOB read, returning garbage (which is then used in
  `stbir__pixel_channels[]` lookup and as a `stbir_internal_pixel_layout`
  enum, leading to undersized allocations or wrong pixel processing).
- **Reproduction sketch:**
  ```c
  stbir_set_pixel_layouts(&resize, (stbir_pixel_layout)99, (stbir_pixel_layout)99);
  stbir_build_samplers(&resize);
  ```
- **Status:** Patched
- **Validation test:** `tests/bug_stb_image_resize2_006.c` — UBSan reports index 99 out of bounds for `stbir__pixel_layout_convert_public_to_internal[17]`
- **Fix:** Added pixel_layout validation in `stbir__perform_build` at lines 7950-7951. Returns 0 when `input_pixel_layout_public` or `output_pixel_layout_public` exceed `STBIRI_AR_PM` (16), preventing OOB reads into `stbir__pixel_layout_convert_public_to_internal[]`.

## BUG-stb_image_resize2-007

- **Library:** `stb_image_resize2.h`
- **Severity:** High
- **Class:** Out-of-Bounds Array Index
- **Location:** `stb_image_resize2.h:904-906` (array def), `stb_image_resize2.h:4625,7533,7536,7539,8108` (uses)
- **Source:** static-analysis
- **Technique:** static-analysis
- **Description:**
  The array `stbir__type_size[]` has exactly 6 entries (indices 0-5 for
  valid `stbir_datatype` values 0-5). The `data_type` / `input_type` /
  `output_type` parameters are used to index this array at multiple
  locations with no bounds check. A value > 5 reads garbage from memory
  adjacent to the array. When used in allocation or stride calculation
  (lines 7533, 7536, 7539), a wrong `type_size` leads to undersized
  buffers and consequent heap or stack buffer overflows.
- **Reproduction sketch:**
  ```c
  stbir_resize(input, 100, 100, 0, output, 100, 100, 0,
               STBIR_RGBA, (stbir_datatype)255,
               STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT);
  ```
- **Status:** Patched
- **Validation test:** `tests/bug_stb_image_resize2_007.c` — UBSan reports index 255 out of bounds for `unsigned char[6]`.
- **Fix:** Added `data_type` bounds checks in `stbir_quick_resize_helper` (line 8112) and `stbir__perform_build` (lines 7952-7953). Returns 0 when `data_type` > `STBIR_TYPE_HALF_FLOAT` (5), preventing OOB reads from `stbir__type_size[]`.

## BUG-stb_image_resize2-008

- **Library:** `stb_image_resize2.h`
- **Severity:** Medium
- **Class:** Missing Input Validation (data_type)
- **Location:** `stb_image_resize2.h:7802`
- **Source:** static-analysis
- **Technique:** static-analysis
- **Description:**
  `stbir_set_datatypes()` stores `input_type` and `output_type` directly
  into the `resize` struct without validating they fall within the valid
  range (0–5). When samplers already exist and do not need rebuilding,
  it calls `stbir__update_info_from_resize` directly (bypassing the
  type validation added by BUG-007 in `stbir__perform_build`), which
  indexes `stbir__type_size[input_type]` and
  `decode_simple[input_type - STBIR_TYPE_UINT8_SRGB]` — both OOB reads.
- **Reproduction sketch:**
  ```c
  stbir_resize_init(&resize, ..., STBIR_TYPE_UINT8);
  stbir_build_samplers(&resize);
  stbir_set_datatypes(&resize, (stbir_datatype)99, (stbir_datatype)99);
  ```
- **Status:** Patched
- **Validation test:** `tests/bug_stb_image_resize2_008.c` — UBSan reports index 99 out of bounds for `unsigned char[6]` in `stbir__type_size`.
- **Fix:** Added bounds checks at the top of `stbir_set_datatypes`: `if ((unsigned)input_type > STBIR_TYPE_HALF_FLOAT) return;` (same for output_type).

## BUG-stb_image_resize2-009

- **Library:** `stb_image_resize2.h`
- **Severity:** High
- **Class:** Integer Overflow → Heap Buffer Overflow
- **Location:** `stb_image_resize2.h:7532-7536`
- **Source:** static-analysis
- **Technique:** static-analysis
- **Description:**
  When `input_stride_bytes == 0` or `output_stride_bytes == 0`, the
  default stride is computed at lines 7532-7536 as:
  ```c
  info->output_stride_bytes = info->channels *
      info->horizontal.scale_info.output_sub_size *
      stbir__type_size[output_type];
  ```
  All three operands are `int`; the multiplication is signed `int * int *
  int` with no overflow check. If the product exceeds `INT_MAX`, undefined
  behavior occurs and the wrapped stride (possibly negative or very small)
  is stored. Downstream, pointer arithmetic such as:
  ```c
  info->output_data = (char*)resize->output_pixels +
      (size_t)info->offset_y * (size_t)resize->output_stride_in_bytes + ...
  ```
  will compute a completely wrong address, allowing OOB reads and writes
  to heap memory.
- **Reproduction sketch:**
  ```c
  // output_sub_size around 1e9 * 4 channels * 1 byte = 4GB > INT_MAX
  stbir_resize(input, 100, 100, 0, output, 0x40000001, 100, 0,
               STBIR_RGBA, STBIR_TYPE_UINT8,
               STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT);
  ```
- **Status:** Invalid
- **Invalid reason:** The overflow path at line 7547 is already blocked before it can be reached. The scanline-size check in `stbir_quick_resize_helper` (line 8133, `scanline_output > 0x7fffffff`) rejects this overflow for the medium API. The `coefficients_size > 0x7fffffff` check in `stbir__set_sampler` (line 6672, added by BUG-011) blocks it for the extended API. Tested with ASan+UBSan via medium API, extended API, and point-sample extended API — no signed integer overflow detected in any path.

## BUG-stb_image_resize2-010

- **Library:** `stb_image_resize2.h`
- **Severity:** High
- **Class:** Integer Overflow → Wrong Pointer Arithmetic / OOB Access
- **Location:** `stb_image_resize2.h:7539`
- **Source:** static-analysis
- **Technique:** static-analysis
- **Description:**
  The output-pointer calculation at line 7539 is:
  ```c
  info->output_data = ( (char*) resize->output_pixels ) +
      ( (size_t) info->offset_y * (size_t) resize->output_stride_in_bytes ) +
      ( info->offset_x * info->channels * stbir__type_size[output_type] );
  ```
  The term `info->offset_x * info->channels * stbir__type_size[output_type]`
  is computed in `int` arithmetic (no casts to `size_t` until after the
  multiplication completes). If `offset_x` is large (e.g. when processing
  a sub-region of a large image), this can overflow `int`, producing a
  small or negative offset that is then added to the pointer as a `size_t`.
- **Reproduction sketch:**
  ```c
  // Pass a large sub-region offset via the extended API
  stbir_resize_region(input, ..., offset_x=0x4000000, ...);
  ```
  (Requires the extended API path that sets `offset_x` to a large value.)
- **Status:** Invalid
- **Invalid reason:** The overflow path `info->offset_x * info->channels * stbir__type_size[output_type]` at line 7550 requires `offset_x > 536870911` (for RGBA UINT8). But `offset_x` is set from `resize->output_subx`, which is clipped to be < `output_w` by `stbir__calculate_region_transform` (line 7723). So `output_w > 536870911` is required, which is blocked by the `coefficients_size > 0x7fffffff` check in `stbir__set_sampler` (line 6672, added by BUG-011). Tested with ASan+UBSan via medium API and extended API — no signed integer overflow detected in any path.


## BUG-stb_image_resize2-011

- **Library:** `stb_image_resize2.h`
- **Severity:** High
- **Class:** Signed Integer Overflow → Undersized Allocation
- **Location:** `stb_image_resize2.h:6670`
- **Source:** static-analysis
- **Technique:** static-analysis
- **Description:**
  The `coefficients_size` field is computed at line 6670 as:
  ```c
  samp->coefficients_size = samp->num_contributors *
      samp->coefficient_width * sizeof(float) +
      sizeof(float) * STBIR_INPUT_CALLBACK_PADDING;
  ```
  The sub-expression `samp->num_contributors * samp->coefficient_width` is
  signed `int` multiplication. If `num_contributors` (which equals the
  output width for gather mode, up to user-controlled values) and
  `coefficient_width` (which can be several thousand in extreme downscales)
  produce a product > `INT_MAX`, signed overflow occurs. The wrapped result
  is then multiplied by `sizeof(float)` (4), producing an allocation size
  that is far too small. The caller (`stbir__alloc_internal_mem_and_build_samplers`)
  uses this value to allocate memory, and subsequent coefficient writes
  overflow the heap buffer.
- **Reproduction sketch:**
  ```c
  stbir_resize(input, 200000000, 1, 0, output, 20000, 1, 0,
               STBIR_RGBA, STBIR_TYPE_UINT8,
               STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT);
  ```
- **Status:** Patched
- **Validation test:** `tests/bug_stb_image_resize2_011.c` — UBSan reports signed integer overflow in coefficient_size calculation.
- **Fix:** Changed `stbir__set_sampler` to return `int` (0 on failure). Added `stbir__mul_add_overflow_check` to protect `coefficients_size` and `gather_prescatter_coefficients_size` computations. Added guard against overflow in the gather prescatter path as well.

## BUG-stb_image_resize2-012

- **Library:** `stb_image_resize2.h`
- **Severity:** Medium
- **Class:** Float-to-Integer Overflow → Undefined Behavior
- **Location:** `stb_image_resize2.h:3019-3022`, `stb_image_resize2.h:3038-3042`
- **Source:** static-analysis
- **Technique:** static-analysis
- **Description:**
  Functions `stbir__get_filter_pixel_width` and `stbir__get_coefficient_width`
  cast the result of `STBIR_CEILF(...)` directly to `int`. When the scale
  is extremely small (e.g. `1e-30`), the expression
  `support(scale, user_data) * 2.0f / scale` produces a float larger than
  `INT_MAX` (2³¹-1). The cast `(int)huge_float` is undefined behavior in C
  when the value is outside the representable range of `int`. On some
  compilers this saturates (INT_MAX or INT_MIN), while on others it may
  produce a small value or trap. A wrong coefficient width leads to
  allocation mismatches and buffer overflows.
- **Reproduction sketch:**
  ```c
  // Upscale from a very small input to a large output
  stbir_resize(input, 1, 10000, 0, output, 10000, 10000, 0,
               STBIR_RGBA, STBIR_TYPE_FLOAT,
               STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT);
  ```
  The `inv_scale` for the vertical pass would be 10000/1 = 10000, and
  `support(1.0f/inv_scale, ...) * 2.0f / inv_scale` ... actually that
  wouldn't produce a huge float. Better:
  ```c
  // Downscale from huge input to tiny output
  stbir_resize(input, 1000000000, 1000000000, 0, output, 1, 1, 0, ...);
  // Here scale = 1/1e9 = 1e-9, inv_scale = 1e9
  // support(scale, ...) * 2.0 / scale = ~2.0 * 2.0 / 1e-9 = 4e9 > INT_MAX
  ```
- **Status:** Invalid
- **Invalid reason:** The extreme downscale path (input_w=2000000000, output_w=1) required for the float-to-int overflow is blocked before reaching `stbir__get_coefficient_width`. The medium API returns NULL due to `scanline_input > 0x7fffffff` check in `stbir_quick_resize_helper`. The extended API build_samplers returns 0 due to `coefficients_size > 0x7fffffff` check (BUG-011) in `stbir__set_sampler`. Tested with ASan+UBSan — no float-to-int overflow detected.

## BUG-stb_image_resize2-013

- **Library:** `stb_image_resize2.h`
- **Severity:** Medium
- **Class:** Float Underflow → Division by Zero / Infinite Scale
- **Location:** `stb_image_resize2.h:7721-7726`
- **Source:** static-analysis
- **Technique:** static-analysis
- **Description:**
  In `stbir__calculate_region_transform`, line 7724:
  ```c
  scale = ( output_range / input_range ) * ratio;
  ```
  When `input_range` is vastly larger than `output_range` (e.g.
  `output_full_range=1, input_full_range=INT_MAX`), the double division
  can underflow to 0.0. Then on line 7726:
  ```c
  scale_info->inv_scale = (float)( 1.0 / scale );
  ```
  `1.0 / 0.0` produces `+inf`, which when cast to `float` remains `+inf`.
  Downstream, `support(inf, ...)` is called with infinite radius, which
  may return an unbounded value, causing `stbir__get_coefficient_width`
  to compute a coefficient width that is either enormous (causing
  allocation failure or OOM) or overflow when cast to `int`.
- **Reproduction sketch:**
  ```c
  stbir_resize(input, 0x7fffffff, 100, 0, output, 1, 100, 0,
               STBIR_RGBA, STBIR_TYPE_FLOAT,
               STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT);
  ```
- **Status:** Invalid
- **Invalid reason:** The float underflow path (input_w=0x7fffffff, output_w=1) is blocked before reaching `stbir__calculate_region_transform`. The medium API returns NULL due to `scanline_input > 0x7fffffff` check in `stbir_quick_resize_helper`. The extended API build_samplers returns 0 due to `coefficients_size > 0x7fffffff` check (BUG-011) in `stbir__set_sampler`. Tested with ASan+UBSan — no float underflow/infinity detected.

## BUG-stb_image_resize2-014

- **Library:** `stb_image_resize2.h`
- **Severity:** Medium
- **Class:** Missing Validation in `stbir__perform_build`
- **Location:** `stb_image_resize2.h:7944-7945`
- **Source:** static-analysis
- **Technique:** static-analysis
- **Description:**
  `stbir__perform_build()` validates horizontal/vertical filter enums
  with a bounds check at lines 7944-7945:
  ```c
  STBIR_RETURN_ERROR_AND_ASSERT( (unsigned)resize->horizontal_filter >= STBIR_FILTER_OTHER)
  STBIR_RETURN_ERROR_AND_ASSERT( (unsigned)resize->vertical_filter >= STBIR_FILTER_OTHER)
  ```
  but does NOT similarly validate `horizontal_edge`, `vertical_edge`,
  `input_pixel_layout_public`, `output_pixel_layout_public`, or
  `input_type`/`output_type`. The missing validation allows arbitrary
  enum values to reach the lookup arrays described in BUG-004 through
  BUG-007. Adding a single bounds check in this central build function
  would catch all invalid enum values at once.
- **Reproduction sketch:**
  Any of BUG-004/BUG-005/BUG-006/BUG-007 triggers through the resize API.
- **Status:** Patched
- **Fix:** Added bounds checks in `stbir__perform_build` for `horizontal_edge`, `vertical_edge`, `input_pixel_layout_public`, `output_pixel_layout_public`, `input_data_type`, and `output_data_type` (the edge/pixel_layout fixes from BUG-004/006/007 collectively address this gap).

## BUG-stb_image_resize2-015

- **Library:** `stb_image_resize2.h`
- **Severity:** Medium
- **Class:** Undefined Behavior (Misaligned Memory Access)
- **Location:** `stb_image_resize2.h:3746`
- **Source:** fuzzer crash (misaligned `stbir_uint64` store)
- **Technique:** fuzzing
- **Description:**
  In `stbir__pack_coefficients`, the `STBIR_MOVE_2` macro (line 3710)
  casts a `float*` to `stbir_uint64*` and performs a direct 8-byte
  store: `((stbir_uint64*)(dest))[0] = ((stbir_uint64*)(src))[0]`.
  When `widest == 3` (coefficient width of 3 floats, 12 bytes), the
  destination pointer `pc` advances by 3 floats per iteration. After
  the first iteration, `pc` is at offset 12 bytes from the base, which
  is not 8-byte aligned (12 mod 8 = 4). Every subsequent odd-numbered
  iteration writes to a misaligned address. The coefficient buffer is
  allocated via `STBIR__NEXT_PTR` (16-byte aligned), but the 12-byte
  stride causes misalignment after the first iteration.
  On ARM platforms this causes a SIGBUS; on x86 the hardware handles
  it (with a performance penalty), but it is undefined behavior per
  the C standard.
- **Reproduction sketch:**
  ```c
  // Any 3-channel resize where coefficient_width == 3
  // (e.g., 3x3 → 9x9 with a cubic filter and reflect edge mode)
  stbir_resize(input, 3, 3, 0, output, 9, 9, 0,
               STBIR_RGBA, STBIR_TYPE_UINT8,
               STBIR_EDGE_REFLECT, STBIR_FILTER_CUBICBSPLINE);
  ```
- **Status:** Patched
- **Validation test:** `tests/bug_stb_image_resize2_015.c` — UBSan reports store to misaligned address for type `stbir_uint64` at stb_image_resize2.h:3746.
- **Fix:** Replaced `((stbir_uint64*)(dest))[0] = ((stbir_uint64*)(src))[0];` with `memcpy( dest, src, sizeof(stbir_uint64) );` in the `STBIR_MOVE_2` macro at line 3710. Using memcpy avoids the undefined behavior of casting `float*` to `stbir_uint64*` on a potentially misaligned address, while producing identical machine code on x86.

## BUG-stb_image_resize2-016

- **Library:** `stb_image_resize2.h`
- **Severity:** Medium
- **Class:** Undefined Behavior (Misaligned Memory Access)
- **Location:** `stb_image_resize2.h:8744` (and 8532)
- **Source:** fuzzer crash (misaligned `int` store)
- **Technique:** fuzzing
- **Description:**
  In the non-SIMD fallback path of `stbir__encode_uint8_linear` (and
  likely other `stbir__encode_*` functions sharing the same code
  pattern), the block-of-4 optimization loop writes:
  ```c
  *(int*)(output-4) = stbir__simdi_to_int( i0 );
  ```
  This casts an `unsigned char*` to `int*` and stores 4 bytes,
  requiring 4-byte alignment. However, the output buffer pointer is
  computed per scanline as:
  ```c
  (char*)stbir_info->output_data + n * stbir_info->output_stride_bytes
  ```
  When `output_stride_bytes` is not a multiple of 4 (e.g., packed BGR
  images where stride = width × 3, or packed 2-channel images where
  stride = width × 2), odd-numbered scanlines land on addresses
  misaligned for `int`. On ARM this causes SIGBUS; on x86 it is
  silently handled but still undefined behavior.
- **Reproduction sketch:**
  ```c
  // BGR (3-channel) image with default packed stride
  // output stride = 2 * 3 * 1 = 6 bytes/scanline (not a multiple of 4)
  stbir_resize(input, 2, 10, 0, output, 2, 10, 0,
               STBIR_BGR, STBIR_TYPE_UINT8,
               STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT);
  ```
- **Status:** Patched
- **Validation test:** `tests/bug_stb_image_resize2_016.c` — UBSan reports store to misaligned address for type 'int' at stb_image_resize2.h:8744.
- **Fix:** Replaced `*(int*)(output-4) = stbir__simdi_to_int( i0 );` with `{ int stbir__tmp_i = stbir__simdi_to_int( i0 ); memcpy( output-4, &stbir__tmp_i, sizeof(int) ); }` at lines 8532 and 8744. Using memcpy avoids the undefined behavior of casting `unsigned char*` to `int*` on a potentially misaligned address, while producing identical machine code on x86.

## Session Summary — 2026-06-17

| Bug ID | Severity | Class | Status | Notes |
|--------|----------|-------|--------|-------|
| BUG-stb_image_resize2-001 | High | Crash / DoS | Patched | Point sampling hardened |
| BUG-stb_image_resize2-002 | High | Integer Overflow | Patched | Overflow checks added to alloc logic |
| BUG-stb_image_resize2-003 | High | Undefined Behavior | Patched | INT_MIN guard at stbir_quick_resize_helper:8133 |
| BUG-stb_image_resize2-004 | Critical | OOB Function Pointer Call | Patched | Edge validation at stbir__perform_build:7948-7949 |
| BUG-stb_image_resize2-005 | High | Missing Input Validation | Patched | Bounds check added in stbir_set_edgemodes:7842-7843 |
| BUG-stb_image_resize2-006 | High | OOB Array Index | Patched | Pixel layout validation at stbir__perform_build:7950-7951 |
| BUG-stb_image_resize2-007 | High | OOB Array Index | Patched | Data type validation at stbir__quick_resize_helper:8112 & stbir__perform_build:7952-7953 |
| BUG-stb_image_resize2-008 | Medium | Missing Input Validation | Patched | Bounds check added in stbir_set_datatypes:7804-7805 |
| BUG-stb_image_resize2-009 | High | Integer Overflow | Invalid | Pre-blocked by BUG-011's coefficients_size check |
| BUG-stb_image_resize2-010 | High | Integer Overflow | Invalid | Pre-blocked by BUG-011's coefficients_size check |
| BUG-stb_image_resize2-011 | High | Signed Integer Overflow | Patched | Overflow-safe coefficients_size at stbir__set_sampler:6672-6675 |
| BUG-stb_image_resize2-012 | Medium | Float-to-Int Overflow | Invalid | Pre-blocked by scanline_input and coefficients_size checks |
| BUG-stb_image_resize2-013 | Medium | Float Underflow / Infinity | Invalid | Pre-blocked by scanline_input and coefficients_size checks |
| BUG-stb_image_resize2-014 | Medium | Missing Validation | Patched | Added edge, pixel_layout, data_type validation in stbir__perform_build |
| BUG-stb_image_resize2-015 | Medium | Misaligned uint64 | Patched | STBIR_MOVE_2: cast+store → memcpy at stb_image_resize2.h:3710 |
| BUG-stb_image_resize2-016 | Medium | Misaligned int store | Patched | Non-SIMD encode: cast+store → memcpy at stb_image_resize2.h:8532, 8744 |

## BUG-stb_image_resize2-017

- **Library:** `stb_image_resize2.h`
- **Severity:** High
- **Class:** Out-of-Bounds Table Index
- **Location:** `stb_image_resize2.h:8922`
- **Source:** fuzzer crash (crash-7963acff5519cf507132bc3fcad6877de2c4fe06, crash-c459c94399ed67a997764b7669a79d5060d5133d)
- **Technique:** fuzzing
- **Description:**
  In the SIMD encode path of `stbir__encode_uint8_srgb` (and similarly
  `stbir__encode_uint8_srgb4_linearalpha` at line 9027), the
  `stbir__min_max_shift20` macro clamps float values to
  `[almost_zero, almost_one]` and shifts right by 20 to produce a
  table index for `fp32_to_srgb8_tab4[104]`. The table is accessed
  via a base pointer offset of `-(127-13)*8 = -912`, so valid shift
  results must be in `[912, 1015]`.
  
  When `STBIR_ASSERT` is disabled (as in fuzzing builds), the library
  can reach the encode path with intermediate float values containing
  NaN (from degenerate resampling with edge WRAP on small images).
  The SIMD `_mm_max_ps` clamp of NaN should return the non-NaN
  operand, but the UBSan report shows index `-912`, meaning the shift
  result is 0. This indicates the clamp did not elevate the value
  above `almost_zero`, allowing the OOB table access.
  
  This crashes under UBSan with `abort_on_error=1` and causes
  undefined behavior in production builds.
- **Reproduction sketch:**
  ```c
  #define STBIR_ASSERT(x) ((void)0)
  #define STB_IMAGE_RESIZE_IMPLEMENTATION
  #include "stb_image_resize2.h"
  #include <string.h>
  #include <stdlib.h>
  int main() {
      unsigned char *in = malloc(3*3*4);
      unsigned char *out = malloc(9*9*4);
      memset(in, 0, 3*3*4);
      memset(out, 0, 9*9*4);
      stbir_resize(in, 3, 3, 0, out, 9, 9, 0,
                   STBIR_RGBA, STBIR_TYPE_UINT8,
                   STBIR_EDGE_WRAP, STBIR_FILTER_DEFAULT);
      free(in); free(out);
      return 0;
  }
  ```
  Compile with: `clang -I. -g -O1 -fsanitize=address,undefined -o test test.c -lm`
  Run with: `UBSAN_OPTIONS=halt_on_error=1:abort_on_error=1 ./test`
- **Status:** Validated
- **Validation test:** `tests/bug_stb_image_resize2_017.c` — UBSan reports "index -912 out of bounds for type 'const stbir_uint32[104]'".
