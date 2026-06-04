# BUGS_stb_hexwave

## BUG-STB_HEXWAVE-001

- **Library:** `stb_hexwave.h`
- **Severity:** Medium
- **Class:** Logic Error / Audio Artifact
- **Location:** `stb_hexwave.h:437-450`
- **Source:** https://github.com/nothings/stb/pull/1684 / https://github.com/nothings/stb/issues/1685
- **Technique:** web-search
- **Description:**
  In `hexwave_generate_samples`, the frequency-change BLAMP fixup is applied to `output[]` at lines 444-445, but then `memset(output, 0, ...)` at line 450 zeros out the entire output buffer, destroying the fixup. The memset and subsequent operations should happen before the frequency-change block, not after. This means frequency discontinuities are not properly bandlimited, causing audible artifacts when the oscillator frequency changes between calls.
- **Reproduction sketch:**
  Call `hexwave_generate_samples` twice with different `freq` values and observe that the BLAMP fixup from the second call is overwritten by memset. The output will contain aliasing artifacts at frequency transitions.
- **Status:** Patched
- **Validation:** `tests/bug_stb_hexwave_001.c` sets up two oscillators with identical state (same `hex->t`, same `hex->buffer`, same waveform parameters) but different `prev_dt`. The oscillator with `prev_dt != dt` triggers the BLAMP fixup; the one with `prev_dt == dt` does not. Both are called with the same `freq` so the synthesis (including transition bleps) is identical. The test compares `output[0..halfw-1]` between the two. With the bug, the BLAMP is erased by `memset(output, 0, ...)` at line 450, so the outputs are identical (`diff_count=0`). With the fix, the BLAMP is preserved in the "with change" case, so the outputs differ. The test exits 1 (failure) against the unpatched library, confirming the bug.
- **Fix:** Moved the frequency-change BLAMP fixup block from before the `memset(output, 0, ...)` to after the `memcpy(output, hex->buffer, ...)` at `stb_hexwave.h:437-447`. The BLAMP is now applied after the buffered data is restored, so it adds to the memcpy result instead of being erased. The test now passes (diff_count=11, max_diff=0.024) against the patched library.

## BUG-STB_HEXWAVE-002

- **Library:** `stb_hexwave.h`
- **Severity:** High
- **Class:** Memory Management (Double-Free / Memory Leak)
- **Location:** `stb_hexwave.h:546-554`
- **Source:** static-analysis
- **Technique:** web-search
- **Description:**
  `hexwave_shutdown` has an inverted condition. When `user_buffer == 0`, `hexwave_init` heap-allocates `blep` and `blamp` buffers via `malloc`. When `user_buffer != 0`, these buffers point into the user-provided buffer. However, `hexwave_shutdown` frees `blep`/`blamp` when `user_buffer != 0` (wrong — these are sub-allocations of the user buffer and must not be freed) and does nothing when `user_buffer == 0` (causing a memory leak of the heap-allocated buffers). This means:
  - Calling `hexwave_shutdown(NULL)` leaks the BLEP/BLAMP tables.
  - Calling `hexwave_shutdown(non_null_buffer)` attempts to `free()` pointers inside the user's buffer, which is undefined behavior (likely a crash or double-free).
- **Reproduction sketch:**
  ```c
  hexwave_init(32, 16, NULL);
  hexwave_shutdown(NULL); // leaks blep and blamp buffers
  ```
- **Status:** Patched
- **Fix:** Inverted the condition at `stb_hexwave.h:549` from `if (user_buffer != 0)` to `if (user_buffer == 0)`. When `user_buffer` is NULL the library owns the heap-allocated BLEP/BLAMP tables and must free them. When `user_buffer` is non-NULL, these pointers are sub-allocations within the user's buffer and must not be freed individually.

## BUG-STB_HEXWAVE-003

- **Library:** `stb_hexwave.h`
- **Severity:** Medium
- **Class:** NULL Pointer Dereference
- **Location:** `stb_hexwave.h:566,579-580`
- **Source:** static-analysis
- **Technique:** web-search
- **Description:**
  `hexwave_init` calls `malloc` at lines 566, 579, and 580 without checking the return value. If any allocation fails (e.g. under low-memory conditions), the subsequent code dereferences the NULL pointers, causing a crash.
- **Reproduction sketch:**
  Cause `malloc` to fail (e.g. via LD_PRELOAD interposition) before calling `hexwave_init(64, 16, NULL)`.
- **Status:** Patched
- **Validation:** `tests/bug_stb_hexwave_003.c` sets `RLIMIT_DATA` to 1GB (large enough for the program to load, small enough that the 16GB allocation in `hexwave_init(64, 33554430, NULL)` fails). The test installs a `SIGSEGV` handler and calls `hexwave_init`. Against the unpatched library, UBSan reports `applying non-zero offset 8589934084 to null pointer` at `stb_hexwave.h:586` (the `step = buffers + 0*n` line where `buffers` is NULL), then the code segfaults when writing to the NULL buffer. The test exits 1, confirming the bug.
- **Fix:** Added NULL checks after each `malloc` call in `hexwave_init` at `stb_hexwave.h:584` and `stb_hexwave.h:592`. If any allocation fails, the function returns early instead of dereferencing the NULL pointer. The test now passes (no crash) against the patched library.

## BUG-STB_HEXWAVE-004

- **Library:** `stb_hexwave.h`
- **Severity:** High
- **Class:** Integer Overflow / Heap Buffer Overflow
- **Location:** `stb_hexwave.h:559-561`
- **Source:** static-analysis
- **Technique:** web-search
- **Description:**
  In `hexwave_init`, allocation size computations use signed `int` arithmetic that can overflow:
  ```c
  int half = halfwidth * oversample;
  int blep_buffer_count = width * (oversample + 1);
  int n = 2 * half + 1;
  ```
  `width` is capped at `STB_HEXWAVE_MAX_BLEP_LENGTH` (default 64), but `oversample` is not validated. A large `oversample` (e.g. ~2^26) causes `half` to overflow to a negative or small value. Consequently `n` becomes negative or small, leading to undersized allocations. The subsequent loops writing to these buffers then overflow heap memory.
- **Reproduction sketch:**
  ```c
  hexwave_init(64, 0x4000000, NULL); // triggers integer overflow
  ```
- **Status:** Patched
- **Fix:** Added bounds checks on `oversample` at `stb_hexwave.h:573-576` before any allocation arithmetic. `oversample` is clamped to 1..33554430, preventing signed integer overflow in `halfwidth*oversample` and `width*(oversample+1)`. Also prevents heap buffer overflow from undersized allocation.

## BUG-STB_HEXWAVE-005

- **Library:** `stb_hexwave.h`
- **Severity:** Informational
- **Class:** Division by Zero
- **Location:** `stb_hexwave.h:590`
- **Source:** static-analysis
- **Technique:** web-search
- **Description:**
  In `hexwave_init`, if `oversample` is 0, the expression `3.141592f * (i - half) / oversample` at line 590 causes a floating-point division by zero, which is undefined behavior. The function does not validate that `oversample >= 1`.
- **Reproduction sketch:**
  ```c
  hexwave_init(32, 0, NULL); // division by zero
   ```
- **Status:** Patched
- **Fix:** Same oversample bounds check as BUG-STB_HEXWAVE-004. The `if (oversample < 1) oversample = 1` guard at `stb_hexwave.h:573` prevents the division by zero.

## Session Summary — 2026-06-04

| Bug ID | Severity | Class | Status | Notes |
|--------|----------|-------|--------|-------|
| BUG-STB_HEXWAVE-001 | Medium | Logic Error / Audio Artifact | Patched | Moved BLAMP fixup after memcpy at stb_hexwave.h:437-447 |
| BUG-STB_HEXWAVE-002 | High | Memory Management | Patched | (pre-existing) Inverted free condition at stb_hexwave.h:549 |
| BUG-STB_HEXWAVE-003 | Medium | NULL Pointer Dereference | Patched | Added NULL checks after malloc at stb_hexwave.h:584,592 |
| BUG-STB_HEXWAVE-004 | High | Integer Overflow / Heap Buffer Overflow | Patched | (pre-existing) Added oversample bounds check at stb_hexwave.h:573-576 |
| BUG-STB_HEXWAVE-005 | Informational | Division by Zero | Patched | (pre-existing) Same oversample bounds check as BUG-004 |
