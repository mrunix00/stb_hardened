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
- **Status:** Unvalidated

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
- **Status:** Unvalidated

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
