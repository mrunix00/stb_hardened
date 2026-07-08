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

## BUG-STB_HEXWAVE-006

- **Library:** `stb_hexwave.h`
- **Severity:** High
- **Class:** Heap Buffer Overflow
- **Location:** `stb_hexwave.h:456`
- **Source:** fuzzer crash (ASan heap-buffer-overflow)
- **Technique:** fuzzing
- **Description:**
  In `hexwave_generate_samples`, the frequency-change BLAMP fixup at line 456 writes `hex_blamp(output, 0, ...)` which writes `hexblep.width` floats to `output[0..bw-1]`. However, when `num_samples < hexblep.width`, the output buffer is smaller than `hexblep.width` floats. The fixup should write to `temp_output` (where all synthesis goes for short outputs) instead of `output`. The BLAMP writes past the end of the output buffer, causing a heap buffer overflow. Additionally, the fixup data is lost because the short-output path only copies `num_samples` floats from `temp_output` to `output`, never incorporating the BLAMP data.
- **Reproduction sketch:**
  ```c
  hexwave_init(32, 16, user_buffer);
  HexWave osc;
  hexwave_create(&osc, 1, 0, 0, 0);
  osc.prev_dt = 0.5f; // different from freq to trigger BLAMP
  float out[4];
  hexwave_generate_samples(out, 4, &osc, 0.1f); // num_samples < hexblep.width
  ```
- **Status:** Patched
- **Fix:** Redirected BLAMP fixup to `temp_output` when `num_samples < hexblep.width` (line 452). Previously wrote to `output` even in short-buffer mode; now selects target buffer based on `num_samples >= hexblep.width`.

## BUG-STB_HEXWAVE-007

- **Library:** `stb_hexwave.h`
- **Severity:** High
- **Class:** Out-of-Bounds Array Access
- **Location:** `stb_hexwave.h:488-494`
- **Source:** fuzzer (UBSan: index 9 out of bounds for hexvert[9])
- **Technique:** fuzzing
- **Description:**
  In `hexwave_generate_samples`, the initial segment-finding loop at line 488 iterates `for (j=0; j < 8; ++j) if (t < vert[j+1].t) break;`. When `t` is NaN or `t >= 1.0` (greater than all segment endpoint times), no segment matches and the loop exits with `j=8`. The subsequent `while (t < vert[j+1].t)` at line 494 then accesses `vert[9]`, which is one past the end of the `hexvert vert[9]` array. When `t` is NaN, `j` keeps incrementing past 8 in the outer loop (since `j == 8` is the only reset condition, and when `j` passes 8 the wrap never triggers), reading further OOB on each iteration.
- **Reproduction sketch:**
  ```c
  hexwave_init(32, 16, user_buffer);
  HexWave osc;
  hexwave_create(&osc, 1, 0, 0, 0);
  osc.t = 1e10f; // or NAN
  float out[128];
  hexwave_generate_samples(out, 128, &osc, 0.1f);
  ```
- **Status:** Patched
- **Fix:** Added wrap-around check after segment-finding loop: when `j == 8` (t >= 1.0), reset `j = 0` and `t -= 1.0f` (line 491). Without this, `vert[9]` was accessed OOB. The wrap logic mirrors the existing wrap at line 508.

## BUG-STB_HEXWAVE-008

- **Library:** `stb_hexwave.h`
- **Severity:** Medium
- **Class:** Undefined Behavior (Float-to-Int Overflow) / Potential OOB Read
- **Location:** `stb_hexwave.h:321`
- **Source:** fuzzer (UBSan: nan outside range of representable int values)
- **Technique:** fuzzing
- **Description:**
  In `hex_add_oversampled_bleplike`, the expression `(int)(time_since_transition * hexblep.oversample)` at line 321 can receive NaN or Infinity values from upstream when `freq` is NaN (propagating through `dt` and `recip_dt`). Converting NaN or Infinity to `int` is undefined behavior; on x86-64 this produces `INT_MIN` (0x80000000). The resulting `slot` value (INT_MIN) then causes `d1 = &data[slot * bw]` where `slot * bw` is a huge negative offset (or wraps to a large positive offset via signed overflow), leading to an out-of-bounds read from the BLEP/BLAMP tables. ASan confirmed a heap-buffer-overflow READ via this path.
- **Reproduction sketch:**
  Call `hexwave_generate_samples` with `freq = NAN` and parameters that cause the main-loop BLEP/BLAMP transitions to fire before the segment-scan walk-off.
- **Status:** Patched
- **Fix:** Clamped float multiplication `time_since_transition * oversample` before the int cast in `hex_add_oversampled_bleplike` (line 321). Used intermediate `float fslot` clamped to `[0, oversample]` range before cast, preventing overflow UB even with extreme time values.

## BUG-STB_HEXWAVE-009

- **Library:** `stb_hexwave.h`
- **Severity:** Low
- **Class:** Denial of Service (Infinite Loop)
- **Location:** `stb_hexwave.h:418-544`
- **Source:** fuzzer (libFuzzer timeout)
- **Technique:** fuzzing
- **Description:**
  When `freq` is NaN, `fabs(NaN) = NaN`, so `dt = NaN` and `t += dt` makes `t = NaN`. The outer `for(;;)` loop's inner `while (t < vert[j+1].t)` condition is always false (NaN comparisons always return false), so the inner loop is never entered. The transition code runs, `j` increments to 8, `t -= 1.0` (still NaN), and this repeats forever. Similarly, when `freq = FLT_MAX` (~3.4e+38), the single inner-loop sample jumps `t` to ~3.4e+38, and after the `j == 8` wrap, `t -= 1.0` does not change the value in float32 precision, locking the loop. The function never returns, causing a denial of service.
- **Reproduction sketch:**
  ```c
  hexwave_generate_samples(out, 128, &osc, INFINITY); // hangs
  ```
- **Status:** Patched
- **Fix:** Added dt guard at `hexwave_generate_samples` entry (line 430): `if (!(dt > 0.0f) || dt > 1.0f) dt = 1.0f;` catches NaN (NaN comparisons are always false), Infinity (fabs(Inf) = Inf, Inf > 1 is true), and FLT_MAX (3.4e38 > 1). Clamping to 1.0 prevents the float32 precision wrap failure (`t -= 1.0f` has no effect on FLT_MAX-scale values) and ensures valid dt for `min_len = dt/256.0f`. Also updated BUG-008's fslot clamp to handle NaN via `!(fslot >= 0.0f)` pattern.

## BUG-STB_HEXWAVE-010

- **Library:** `stb_hexwave.h`
- **Severity:** Medium
- **Class:** Use-After-Free
- **Location:** `stb_hexwave.h:561-569` (`hexwave_shutdown`), `stb_hexwave.h:307-313` (static `hexblep` state)
- **Source:** static-analysis
- **Technique:** static-analysis
- **Description:**
  `hexwave_shutdown` frees `hexblep.blep` and `hexblep.blamp` but does not clear the
  process-global `hexblep` state. After shutdown, `hexblep.width` remains non-zero and
  `hexblep.blep`/`hexblep.blamp` still point into the just-freed heap region. Any later
  call to `hexwave_generate_samples` (for example a deferred or asynchronous audio
  callback that fires after teardown, or a code path that generates samples before a
  re-initialization) reads the freed tables inside `hex_add_oversampled_bleplike`,
  producing a use-after-free. The library leaves no safe "uninitialized" sentinel after
  shutdown, so the dangling global state is silently reused.
- **Reproduction sketch:**
  ```c
  hexwave_init(32, 16, NULL);
  hexwave_shutdown(NULL);                                   // frees blep/blamp, state left dangling
  HexWave osc;
  hexwave_create(&osc, 1, 0.5f, 1.0f, 0.0f);
  float out[256];
  hexwave_generate_samples(out, 200, &osc, 0.3f);          // heap-use-after-free
  ```
- **Status:** Patched
- **Fix:** In `hexwave_shutdown`, after `free(hexblep.blep)` / `free(hexblep.blamp)` in the
  `user_buffer == 0` branch, also clear the global state (`hexblep.blep = hexblep.blamp = 0;`,
  `hexblep.width = hexblep.oversample = 0`). A `hexwave_generate_samples()` call after shutdown
  now runs in the safe `width == 0` degenerate mode instead of dereferencing the freed BLEP/BLAMP
  tables. Change confined to `stb_hexwave.h:561-569`.

## BUG-STB_HEXWAVE-011

- **Library:** `stb_hexwave.h`
- **Severity:** Low
- **Class:** Memory Leak
- **Location:** `stb_hexwave.h:597-611` (`hexwave_init` NULL path)
- **Source:** static-analysis
- **Technique:** static-analysis
- **Description:**
  `hexwave_init` (the `user_buffer == 0` / NULL path) heap-allocates `blep` and `blamp`
  via `malloc`, but it never frees a previous allocation if it is called more than once
  without an intervening `hexwave_shutdown`. Each successive initialization leaks the
  prior `blep`/`blamp` buffers. The global `hexblep` state carries no ownership marker,
  so the library cannot tell whether the currently-held tables are library-allocated
  (must be freed) or sub-allocations of a caller-provided `user_buffer` (must NOT be
  freed). Repeated re-initialization in a long-running process leaks memory.
- **Reproduction sketch:**
  ```c
  hexwave_init(32, 16, NULL);
  hexwave_init(32, 16, NULL);   // first blep/blamp allocation is leaked
  hexwave_shutdown(NULL);       // only frees the second allocation
  ```
  Run under ASan with `detect_leaks=1`; LeakSanitizer reports the first allocation as
  still reachable / leaked.
- **Status:** Invalid
- **Validation:** `tests/bug_stb_hexwave_012.c` drives `hexwave_create`/`hexwave_change` with
  `half_height` = NaN / Inf / -Inf / +/-1e30 and generates samples under ASan+UBSan. No
  memory-safety fault (OOB read/write, UB, leak, or infinite loop) is observed; the only
  effect is non-finite values (NaN/Inf) in the OUTPUT buffer. Segment *times* depend only on
  the clamped `zero_wait`/`peak_time`/`reflect`, so the synthesis loops stay bounded and never
  go out of bounds. Non-finite output is the documented contract limitation (the header states
  "half_height is not clamped" to permit morphs with |h|>1 and negative h, e.g. Sawtooth (8va)
  at h=-1 and crossfades up to h=2). Clamping `half_height` would break those legitimate uses,
  so no fix is applied. Not a security defect -> Invalid.


## BUG-STB_HEXWAVE-012

- **Library:** `stb_hexwave.h`
- **Severity:** Low
- **Class:** Numerical / Quality (NaN & Inf propagation) — not a memory-safety defect
- **Location:** `stb_hexwave.h:241`, `stb_hexwave.h:287-305` (`hexwave_create`/`hexwave_change`), `stb_hexwave.h:355-421` (`hexwave_generate_linesegs`)
- **Source:** static-analysis
- **Technique:** static-analysis
- **Description:**
  `hexwave_create` / `hexwave_change` accept `half_height` without validation — the
  header itself notes "half_height is not clamped". Passing `NaN` or `Inf` produces
  `NaN`/`Inf` in the segment vertices (`vert[].v`) and slopes (`vert[].s`), which
  propagate into the output buffer as `NaN`/`Inf` samples. This is a quality /
  correctness issue, **not** a memory-safety bug: segment *times* (`vert[].t`) depend
  only on `zero_wait`, `peak_time`, and `reflect`, all of which are clamped, so the
  bounded synthesis loops never read or write out of bounds and never loop forever.
  Under ASan+UBSan no fault is observed — the output simply contains non-finite
  values. Documented as a known API contract limitation rather than a security defect.
- **Reproduction sketch:**
  ```c
  HexWave osc;
  hexwave_create(&osc, 1, 0.5f, NAN, 0.0f);
  float out[256];
  hexwave_generate_samples(out, 200, &osc, 0.3f);   // output has NaN, no crash/OOB
  ```
- **Status:** Patched
- **Fix:** Added an `own` flag to the static `hexblep` struct recording whether the library
  heap-owns the BLEP/BLAMP tables (`hexblep.own = (user_buffer == 0) ? 1 : 0;` at the end of
  `hexwave_init`). On re-init, `hexwave_init` now frees the previous library-owned tables
  (`if (hexblep.own) { free(hexblep.blep); free(hexblep.blamp); }`) before allocating new ones,
  eliminating the leak. `hexwave_shutdown` now frees based on `hexblep.own` (instead of the
  `user_buffer` argument) and clears `own` to 0; this also makes it robust against a mismatched
  shutdown argument (e.g. init(buf) then shutdown(NULL) no longer frees user memory). Changes
  confined to `stb_hexwave.h` (struct + `hexwave_init` + `hexwave_shutdown`).


## Session Summary — 2026-06-17

| Bug ID | Severity | Class | Status | Notes |
|--------|----------|-------|--------|-------|
| BUG-STB_HEXWAVE-001 | Medium | Logic Error / Audio Artifact | Patched | Moved BLAMP fixup after memcpy at stb_hexwave.h:437-447 |
| BUG-STB_HEXWAVE-002 | High | Memory Management | Patched | (pre-existing) Inverted free condition at stb_hexwave.h:549 |
| BUG-STB_HEXWAVE-003 | Medium | NULL Pointer Dereference | Patched | Added NULL checks after malloc at stb_hexwave.h:584,592 |
| BUG-STB_HEXWAVE-004 | High | Integer Overflow / Heap Buffer Overflow | Patched | (pre-existing) Added oversample bounds check at stb_hexwave.h:573-576 |
| BUG-STB_HEXWAVE-005 | Informational | Division by Zero | Patched | (pre-existing) Same oversample bounds check as BUG-004 |
| BUG-STB_HEXWAVE-006 | High | Heap Buffer Overflow | Patched | Redirected freq-change BLAMP to temp_output when num_samples < hexblep.width |
| BUG-STB_HEXWAVE-007 | High | Out-of-Bounds Array Access | Patched | Added j==8 wrap after segment-finding loop to prevent vert[9] access |
| BUG-STB_HEXWAVE-008 | Medium | Undefined Behavior (Float-to-Int Overflow) | Patched | Clamped fslot to [0, oversample] before int cast, with NaN guard |
| BUG-STB_HEXWAVE-009 | Low | Denial of Service (Infinite Loop) | Patched | Clamped dt at entry to catch NaN/Inf/FLT_MAX |

## Session Summary — 2026-07-08

| Bug ID | Severity | Class | Status | Notes |
|--------|----------|-------|--------|-------|
| BUG-STB-HEXWAVE-001 | Medium | Logic Error / Audio Artifact | Patched | Moved BLAMP fixup after memcpy at stb_hexwave.h:437-447 (prior session) |
| BUG-STB-HEXWAVE-002 | High | Memory Management | Patched | Inverted free condition fixed (prior session) |
| BUG-STB-HEXWAVE-003 | Medium | NULL Pointer Dereference | Patched | NULL checks after malloc (prior session) |
| BUG-STB-HEXWAVE-004 | High | Integer Overflow / Heap Buffer Overflow | Patched | oversample bounds check (prior session) |
| BUG-STB-HEXWAVE-005 | Informational | Division by Zero | Patched | Same oversample bounds check (prior session) |
| BUG-STB-HEXWAVE-006 | High | Heap Buffer Overflow | Patched | freq-change BLAMP redirected to temp_output (prior session) |
| BUG-STB-HEXWAVE-007 | High | Out-of-Bounds Array Access | Patched | j==8 wrap after segment-finding loop (prior session) |
| BUG-STB-HEXWAVE-008 | Medium | Undefined Behavior (Float-to-Int Overflow) | Patched | fslot clamped before int cast (prior session) |
| BUG-STB-HEXWAVE-009 | Low | Denial of Service (Infinite Loop) | Patched | dt guard at entry (prior session) |
| BUG-STB-HEXWAVE-010 | Medium | Use-After-Free | Patched | Clear hexblep state in hexwave_shutdown (this session) |
| BUG-STB-HEXWAVE-011 | Low | Memory Leak | Patched | Added `own` flag; free previous tables on re-init (this session) |
| BUG-STB-HEXWAVE-012 | Low | Numerical / Quality (NaN & Inf propagation) | Invalid | Non-finite half_height only yields non-finite output; no memory fault (this session) |

Technique: static-analysis. AUTO_PROGRESS session. New findings validated and patched:
- BUG-010 (use-after-free after shutdown): `hexwave_shutdown` now clears the global `hexblep`
  state (blep/blamp = NULL, width = oversample = own = 0) so a post-shutdown
  `hexwave_generate_samples` runs in the safe width==0 degenerate mode instead of reading freed heap.
- BUG-011 (re-init leak): added an `own` ownership flag; `hexwave_init` frees the previous
  library-owned tables on re-init, and `hexwave_shutdown` frees based on `own` (robust against a
  mismatched shutdown argument).
- BUG-012 (unvalidated half_height): confirmed non-security (only non-finite output); left as a
  documented contract limitation, no fix applied.
All 9 pre-existing regression tests plus the 3 new tests pass under ASan+UBSan; no regressions.
