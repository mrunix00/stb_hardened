# BUGS — stb_vorbis.c

---

## BUG-stb_vorbis-001

- **Library:** `stb_vorbis.c`
- **Severity:** High
- **Class:** Heap Buffer Overflow (0-byte write with pre-allocated buffer)
- **Location:** `stb_vorbis.c:3652-3658`
- **Source:** [CVE-2023-45675](https://nvd.nist.gov/vuln/detail/CVE-2023-45675) / GHSL-2023-165
- **Technique:** web-search
- **Description:**
  When parsing the vendor string, `len` is read via `get32_packet(f)`. If `len` is `-1`, then `len+1` evaluates to `0`, and `setup_malloc(f, 0)` is called. When `f->alloc.alloc_buffer` is pre-allocated, `setup_malloc(0)` does *not* return `NULL` — it returns the current `setup_offset` pointer without consuming any space. The subsequent `f->vendor[len] = (char)'\0'` writes the null terminator at index `-1` relative to the pointer, and the for loop writes `f->vendor[i]` for `i=0..len-1` (i.e., 0 iterations for len=-1, but the null write at `f->vendor[-1]` is still OOB). When using `malloc` path instead, `malloc(0)` returns NULL and the function returns early.
- **Reproduction sketch:**
  Craft an Ogg Vorbis file where the vendor string length field (4 bytes at the comment header offset) has the value `0xFFFFFFFF` (-1). Call `stb_vorbis_decode_memory()` with this file using a pre-allocated alloc buffer, or use the heap-based path to see `malloc(0)` → NULL which safely exits but indicates the validation hole.
- **Status:** Patched
- **Test:** `tests/bug_stb_vorbis_001.c` — reproduces with ASan heap-buffer-overflow at `stb_vorbis.c:3658`
- **Fix:** Added `if (len < 0) return error(f, VORBIS_invalid_setup);` before `setup_malloc` at `stb_vorbis.c:3653`. Rejects negative vendor string length, preventing `f->vendor[-1]` OOB write.

---

## BUG-stb_vorbis-002

- **Library:** `stb_vorbis.c`
- **Severity:** High
- **Class:** Heap Buffer Overflow (integer overflow in size alignment)
- **Location:** `stb_vorbis.c:950-961`
- **Source:** [CVE-2023-45676](https://nvd.nist.gov/vuln/detail/CVE-2023-45676) / GHSL-2023-166
- **Technique:** web-search
- **Description:**
  In `setup_malloc`, the expression `(sz+7) & ~7` rounds up allocation size to the nearest 8 bytes for alignment. When `sz` is sufficiently large (close to `INT_MAX`), `sz+7` overflows to a negative value, which after the bitwise AND yields a small positive or zero size. The check `if (f->setup_offset + sz > f->temp_offset)` then passes because the truncated `sz` is small, causing an undersized allocation. The subsequent loop writes `len` bytes (the original large value) into the too-small buffer. The same overflow exists in `setup_temp_malloc` at line 971.
- **Reproduction sketch:**
  Craft an Ogg Vorbis file where the vendor string length is set to a value that causes `sz+7` integer overflow, e.g., `INT_MAX - 3` (0x7FFFFFFC). This passes through `setup_malloc` with a small effective allocation while the loop writes `INT_MAX - 3` bytes.
- **Status:** Patched
- **Test:** `tests/bug_stb_vorbis_002.c` — reproduces with UBSan (signed overflow at `stb_vorbis.c:952`) and ASan (heap-buffer-overflow at `stb_vorbis.c:3657`)
- **Fix:** Added `if (sz < 0 || sz > INT_MAX - 7) return NULL;` at top of both `setup_malloc` (`stb_vorbis.c:951`) and `setup_temp_malloc` (`stb_vorbis.c:971`). Prevents signed integer overflow in the `(sz+7) & ~7` alignment expression, which previously caused undersized allocation in the pre-allocated buffer path.

---

## BUG-stb_vorbis-003

- **Library:** `stb_vorbis.c`
- **Severity:** High
- **Class:** Heap Buffer Overflow / OOB Write (negative index and INT_MAX overflow)
- **Location:** `stb_vorbis.c:3652-3677`
- **Source:** [CVE-2023-45677](https://nvd.nist.gov/vuln/detail/CVE-2023-45677) / GHSL-2023-167
- **Technique:** web-search
- **Description:**
  Two issues in vendor and comment string handling:
  1. **Negative len:** When `get32_packet(f)` returns a negative value, `setup_malloc` may allocate a buffer (e.g., for `len = -1`, `len+1 = 0`). In the pre-allocated buffer path, `setup_malloc(f, 0)` returns a valid pointer (no NULL). The subsequent `f->comment_list[i][len] = '\0'` writes at a negative index, corrupting prior alloc_buffer data (the MSB of the `comment_list[i]` pointer value).
  2. **INT_MAX overflow:** When `len == INT_MAX`, `len+1` wraps to `INT_MIN` (negative), producing either a huge `size_t` allocation (malloc path, likely OOM) or a negative size that wraps to small positive.
- **Reproduction sketch:**
  Craft an Ogg Vorbis file where a comment entry length field is `0xFFFFFFFF` (-1 as int). Load with `stb_vorbis_open_memory()` using a pre-allocated buffer. The OOB write at `comment_list[0][-1] = '\0'` corrupts `alloc_buffer[15]` (the MSB of the first comment string pointer).
- **Status:** Patched
- **Test:** `tests/bug_stb_vorbis_003.c` — detects corruption of alloc_buffer sentinel byte after OOB write
- **Fix:** Added `if (len < 0) return error(f, VORBIS_invalid_setup);` before `setup_malloc` for comment string allocation at `stb_vorbis.c:3680`. This matches the existing guard for vendor string length (BUG-001 fix). The INT_MAX overflow prong is already blocked by the BUG-002 `setup_malloc` alignment guard. With this fix, negative comment string lengths are rejected before reaching the OOB write.

---

## BUG-stb_vorbis-004

- **Library:** `stb_vorbis.c`
- **Severity:** High
- **Class:** Stack Buffer Overflow (submap array OOB write)
- **Location:** `stb_vorbis.c:753-760`, `stb_vorbis.c:4105-4111`
- **Source:** [CVE-2023-45678](https://nvd.nist.gov/vuln/detail/CVE-2023-45678) / GHSL-2023-168
- **Technique:** web-search
- **Description:**
  The `Mapping` struct declares `submap_floor[15]` and `submap_residue[15]`, but the Vorbis spec allows up to 16 submaps (`get_bits(f,4)+1` = 1..16). At line 4105, the loop `for (j=0; j < m->submaps; ++j)` writes into these 15-element arrays. When `m->submaps == 16`, the 16th write at index 15 overflows the stack-embedded arrays by 1 element each.
- **Reproduction sketch:**
  Craft an Ogg Vorbis setup header with a mapping that declares 16 submaps. Load the file; the writes at `submap_floor[15]` and `submap_residue[15]` corrupt adjacent stack memory.
- **Status:** Patched
- **Test:** `tests/bug_stb_vorbis_004.c` — UBSan index 15 out-of-bounds at `stb_vorbis.c:4119` and `:4121`
- **Fix:** Changed `submap_floor[15]` to `submap_floor[16]` and `submap_residue[15]` to `submap_residue[16]` at `stb_vorbis.c:758-759`. Matches spec maximum of 16 submaps (4-bit field 0-15, +1 = 1-16). Validated with UBSan bounds checking (`-fsanitize=bounds`) detecting array index 15 exceeding `uint8[15]` bounds.

---

## BUG-stb_vorbis-005

- **Library:** `stb_vorbis.c`
- **Severity:** High
- **Class:** Use-After-Free / Free of Uninitialized Memory
- **Location:** `stb_vorbis.c:3660-3677`, `stb_vorbis.c:4208-4216`
- **Source:** [CVE-2023-45679](https://nvd.nist.gov/vuln/detail/CVE-2023-45679) / GHSL-2023-169
- **Technique:** web-search
- **Description:**
  When `start_decoder` allocates `f->comment_list` at line 3664, the pointer is set, but the individual `f->comment_list[i]` entries are not initialized to NULL. If a later allocation `f->comment_list[i] = setup_malloc(...)` fails at line 3670 and the function returns early, `vorbis_deinit` at line 4213-4214 iterates `i < f->comment_list_length` and calls `setup_free(p, p->comment_list[i])` on uninitialized pointers, causing a use-after-free / free of wild pointer.
- **Reproduction sketch:**
  Craft an Ogg Vorbis file with `comment_list_length > 0` and a comment entry whose `len` triggers `setup_malloc` failure (e.g., huge size). Run with `ASAN_OPTIONS=allocator_may_return_null=1` to simulate allocation failure; the subsequent `vorbis_deinit` frees uninitialized pointers.
- **Status:** Patched
- **Test:** `tests/bug_stb_vorbis_005.c` — reproduces free-of-uninitialized-memory at `stb_vorbis.c:4238` with ASan (pointer `0xbe…be` from ASan's fill pattern)
- **Fix:** Added `memset(f->comment_list, 0, alloc_size);` after successful `setup_malloc` at `stb_vorbis.c:3683`. Ensures all entries are NULL so `vorbis_deinit` calls `free(NULL)` (safe) on entries that were never allocated due to mid-loop allocation failure.

---

## BUG-stb_vorbis-006

- **Library:** `stb_vorbis.c`
- **Severity:** Medium
- **Class:** Null Pointer Dereference
- **Location:** `stb_vorbis.c:4208-4229`
- **Source:** GHSL-2023-170 / CVE-2023-45680
- **Technique:** web-search
- **Description:**
  If `start_decoder` fails partway through initialization after some resources have been allocated (such as residue configs or codebooks), `vorbis_deinit` may dereference null pointers when iterating over partially-initialized structures. Specifically, if `f->residue_config` is NULL but `f->residue_count` is non-zero, the loop at line 4218 accesses `p->residue_config[i]`.
- **Reproduction sketch:**
  Craft a file that proceeds far enough into `start_decoder` to set `f->residue_count` but then fails before `f->residue_config` is allocated. `vorbis_deinit` iterates the count and dereferences NULL.
- **Status:** Invalid
- **Test:** `tests/bug_stb_vorbis_006.c` — opens a crafted file through heap and pre-allocated paths; confirms no crash occurs
- **Explanation:** The upstream code already contains three guards that prevent the claimed null dereferences: `if (p->residue_config)` at line 4243 guards the residue loop, `if (p->mapping)` at line 4270 guards the mapping loop, and the channel loop at line 4276 is bounded by `STB_VORBIS_MAX_CHANNELS`. These guards were added in commit `0e24ec5f` (2014), predating the CVE assignment. The bug as described is not triggerable in the current codebase.

---

## BUG-stb_vorbis-007

- **Library:** `stb_vorbis.c`
- **Severity:** High
- **Class:** Integer Overflow → Heap Buffer Overflow
- **Location:** `stb_vorbis.c:3664`
- **Source:** [CVE-2023-45681](https://nvd.nist.gov/vuln/detail/CVE-2023-45681) / GHSL-2023-171 / [TALOS-2023-1846](https://talosintelligence.com/vulnerability_reports/TALOS-2023-1846) / CVE-2023-47212
- **Technique:** web-search
- **Description:**
  At line 3664, the allocation `sizeof(char*) * (f->comment_list_length)` uses `size_t` multiplication but `f->comment_list_length` is read as a signed 32-bit int via `get32_packet`. On 32-bit platforms (where `sizeof(char*) == 4`), a large value for `comment_list_length` causes integer overflow in the multiplication, resulting in an undersized allocation. The subsequent loop then writes `comment_list_length` pointers past the allocated buffer's end, achieving heap-buffer-overflow and potentially arbitrary code execution.
- **Reproduction sketch:**
  Craft a file with `comment_list_length = 0x40000001` (1073741825). On 32-bit, `sizeof(char*) * 0x40000001` overflows to 4 bytes, allocating space for 1 pointer, but the loop writes >1 billion pointers.
- **Status:** Patched
- **Fix:** Added overflow guard at `stb_vorbis.c:3664-3666`: compute `alloc_size` in `size_t`, validate that multiplication did not overflow and that result fits in `int` (since `setup_malloc` takes `int`). Also zero `comment_list_length` on early-exit to prevent `vorbis_deinit` from dereferencing the NULL `comment_list` pointer.

---

## BUG-stb_vorbis-008

- **Library:** `stb_vorbis.c`
- **Severity:** Medium
- **Class:** Out-of-Bounds Read (Wild Address Read)
- **Location:** `stb_vorbis.c:1717-1729`, `stb_vorbis.c:1754-1756`, `stb_vorbis.c:3231`
- **Source:** [CVE-2023-45682](https://nvd.nist.gov/vuln/detail/CVE-2023-45682) / GHSL-2023-172
- **Technique:** web-search
- **Description:**
  The `DECODE` macro (which wraps `DECODE_RAW`) allows `var` to become negative: in `DECODE_RAW`, `codebook_decode_scalar_raw` may return -1, and at line 1726, `var` is explicitly set to -1 when `valid_bits` underflows. When `c->sparse` is true, the `DECODE` macro at line 1756 does `var = c->sorted_values[var]`, using a negative index into the `sorted_values` array. Additionally, at line 3231 in `vorbis_decode_packet_rest`, the negative `var` is used as an array index into `finalY` or similar structures, producing wild reads that can leak memory contents.
- **Reproduction sketch:**
  Craft a Vorbis file with a sparse codebook and a huffman decode that produces -1 (e.g., by exhausting valid bits). The -1 propagates through `DECODE` → `sorted_values[-1]`, reading before the array base.
- **Status:** Invalid
- **Test:** `tests/bug_stb_vorbis_008.c` — confirms no crash when opening crafted file with sparse codebook path
- **Explanation:** The upstream code already protects against this via a sentinel at `c->sorted_values[-1] = -1` (lines 3851-3852). The sentinel allocation (`+1` slot at line 3849) ensures `sorted_values[-1]` is a valid memory location initialized to -1. When `DECODE_RAW` returns -1 (EOP), the `DECODE` macro at line 1764 does `var = c->sorted_values[-1] = -1`, which is then skipped by `do_floor`'s `if (finalY[j] >= 0)` check at line 3095. The sentinel was added upstream (commit 48478a1d) specifically for this issue and predates the CVE assignment.

---

## Session Summary — 2026-06-03

| Bug ID | Severity | Class | Status | Notes |
|--------|----------|-------|--------|-------|
| BUG-stb_vorbis-001 | High | Heap Buffer Overflow | Patched | Fixed at stb_vorbis.c:3653 |
| BUG-stb_vorbis-002 | High | Heap Buffer Overflow (alignment overflow) | Patched | Fixed at stb_vorbis.c:951,971 |
| BUG-stb_vorbis-003 | High | Heap Buffer Overflow / OOB Write | Patched | Fixed at stb_vorbis.c:3680 |
| BUG-stb_vorbis-004 | High | Submap Array Overflow | Patched | Fixed at stb_vorbis.c:758-759 |
| BUG-stb_vorbis-005 | High | Use-After-Free / Uninitialized Free | Patched | memset zero-init at :3683 |
| BUG-stb_vorbis-006 | Medium | Null Pointer Dereference | Invalid | Already fixed upstream (guards present since 2014) |
| BUG-stb_vorbis-007 | High | Integer Overflow → Heap Overflow | Patched | Fixed at stb_vorbis.c:3664-3667 |
| BUG-stb_vorbis-008 | Medium | Out-of-Bounds Read (sorted_values[-1]) | Invalid | Sentinels at :3851-3852 already protect |
| BUG-stb_vorbis-009 | High | Heap Buffer Overflow | Patched | Fixed at stb_vorbis.c:3896-3898 |
| BUG-stb_vorbis-010 | Medium | Out-of-Bounds Read | Patched | &255 mask at :3111 |

## Session Summary — 2026-06-04

| Bug ID | Severity | Class | Status | Notes |
|--------|----------|-------|--------|-------|
| BUG-stb_vorbis-004 | High | Submap Array Overflow | Patched | submap_floor/residue[15]→[16] at :758-759 |
| BUG-stb_vorbis-009 | High | Heap Buffer Overflow (size_t→int truncation) | Patched | Added overflow guard setup_malloc size check + lookup1_values UBSan fix |
| BUG-stb_vorbis-005 | High | Use-After-Free / Uninitialized Free | Patched | memset zero-init at :3683 |
| BUG-stb_vorbis-010 | Medium | Out-of-Bounds Read (table index) | Patched | &255 mask at :3111 |
| BUG-stb_vorbis-008 | Medium | Out-of-Bounds Read (sorted_values[-1]) | Invalid | Sentinels at :3851-3852 already protect |

## Session Summary — 2026-06-21 (fuzzing + static analysis)

---

## BUG-stb_vorbis-011

- **Library:** `stb_vorbis.c`
- **Severity:** High
- **Class:** Undefined Behavior (Signed Left Shift of Negative Value)
- **Location:** `stb_vorbis.c:1594-1595`
- **Source:** libFuzzer + UBSan discovery
- **Technique:** fuzzing
- **Description:**
  `get32_packet` assembles a 32-bit value from four `get8_packet` calls. When `get8_packet` returns `EOP` (-1) on end-of-packet, lines 1594-1595 perform `(-1) << 8` and `(-1) << 16`, which are undefined behavior per C99 §6.5.7 (left-shifting a negative `int`). Line 1596 is safe because it casts to `uint32` first. The UB can cause the compiler to produce unexpected results or silently miscompile code that depends on the assembled value.
- **Reproduction sketch:**
  Feed a truncated or malformed Vorbis stream that causes `get8_packet` to return EOP during any `get32_packet` call (e.g. comment header parsing at :3660, setup parsing). UBSan reports: `stb_vorbis.c:1594:24: runtime error: left shift of negative value -1`.
- **Status:** Patched
- **Test:** `tests/bug_stb_vorbis_011.c` — UBSan reports left shift of negative value at :1594 and :1595
- **Fix:** Cast `get8_packet(f)` to `uint32` before shifting on lines 1594-1595 in `get32_packet`, matching the existing cast on line 1596.

---

## BUG-stb_vorbis-012

- **Library:** `stb_vorbis.c`
- **Severity:** High
- **Class:** Signed Integer Overflow → Undefined Behavior
- **Location:** `stb_vorbis.c:3662`
- **Source:** libFuzzer + UBSan discovery
- **Technique:** fuzzing
- **Description:**
  In comment header parsing, `len = get32_packet(f)` reads the vendor string length. The check `if (len < 0)` rejects negative values, but when `len = INT_MAX` (0x7FFFFFFF), the expression `sizeof(char) * (len+1)` at line 3662 computes `len+1` in `int` arithmetic, overflowing to `INT_MIN`. This is signed integer overflow (undefined behavior). The same pattern exists at line 3689 for comment string lengths.
- **Reproduction sketch:**
  Craft an Ogg Vorbis file where the vendor string length field is `0x7FFFFFFF`. Call `stb_vorbis_decode_memory()`. UBSan reports: `stb_vorbis.c:3662:59: runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'`.
- **Status:** Patched
- **Test:** `tests/bug_stb_vorbis_012.c` — UBSan reports signed integer overflow at :3662
- **Fix:** Added `len > INT_MAX - 1` overflow guard to both vendor string length check (:3661) and comment string length check (:3688). Prevents `len+1` from overflowing when `len = INT_MAX`.

---

## BUG-stb_vorbis-013

- **Library:** `stb_vorbis.c`
- **Severity:** Medium
- **Class:** Data Corruption (Off-by-Error in Clamping Formula)
- **Location:** `stb_vorbis.c:1899`
- **Source:** Static analysis
- **Technique:** static-analysis
- **Description:**
  In `codebook_decode_deinterleave_repeat`, the bounds-clamping formula at line 1899 has a sign error. The intent is to compute remaining elements as `len*ch - (p_inter*ch + c_inter)`, but the code computes `len*ch - (p_inter*ch - c_inter)`, which is too large by `2 * c_inter`. When `c_inter > 0` and the clamping condition triggers, the decode loop writes `2*c_inter` extra elements past the intended residue region boundary, corrupting the IMDCT input region of the channel buffer.
- **Reproduction sketch:**
  Craft a Vorbis stream with residue type 2, ch > 1, a codebook where `dimensions > 1`, and `c_inter > 0` when the clamping condition triggers. The over-write corrupts audio output.
- **Status:** Patched
- **Test:** `tests/bug_stb_vorbis_013.c` — analytical proof: buggy effective=3 vs correct=1 for ch=2, len=256, p_inter=255, c_inter=1
- **Fix:** Changed `p_inter*ch - c_inter` to `p_inter*ch + c_inter` at :1899. Corrects the remaining-elements calculation in `codebook_decode_deinterleave_repeat` clamping.

---

## BUG-stb_vorbis-014

- **Library:** `stb_vorbis.c`
- **Severity:** Medium
- **Class:** Temp Buffer OOB Write (latent, compile-flag dependent)
- **Location:** `stb_vorbis.c:2158-2161`
- **Source:** Static analysis
- **Technique:** static-analysis
- **Description:**
  When `STB_VORBIS_DIVIDES_IN_RESIDUE` is defined, the `classifications` temp buffer is allocated with `part_read` elements, but the unpacking loop writes at index `i + pcount` where `i` goes from `classwords-1` to 0. If `classwords > 1` and `pcount` is near `part_read`, this writes beyond the allocated sub-array. The buffer is stack-allocated via `alloca()` by default, so this can corrupt adjacent stack frames.
- **Reproduction sketch:**
  Compile with `-DSTB_VORBIS_DIVIDES_IN_RESIDUE`. Craft a stream where the classbook has `dimensions > 1` and `part_read` is not a multiple of `classwords`.
- **Status:** Patched
- **Test:** `tests/bug_stb_vorbis_014.c` — compiled with STB_VORBIS_DIVIDES_IN_RESIDUE, analytically confirmed OOB when classwords doesn't divide part_read
- **Fix:** Added `if (pcount + i < part_read)` bounds check before each `classifications[j][i+pcount]` write in all three residue decode paths (ch==2, ch>2, general).

---

## BUG-stb_vorbis-015

- **Library:** `stb_vorbis.c`
- **Severity:** Low
- **Class:** Signed Integer Overflow
- **Location:** `stb_vorbis.c:5395`
- **Source:** Static analysis
- **Technique:** static-analysis
- **Description:**
  In the decode-to-short convenience function, `total *= 2` (line 5395) is a signed `int` multiplication that can overflow after repeated doublings. When `total` reaches 2^30, the next doubling overflows to a negative value. Converting to `size_t` produces a huge allocation request. The `realloc` failure is handled gracefully (returns -2), but the signed overflow itself is undefined behavior.
- **Reproduction sketch:**
  Decode an extremely long Vorbis stream via `stb_vorbis_decode_filename()` or `stb_vorbis_decode_memory()` — requires ~2 billion samples to trigger.
- **Status:** Patched
- **Test:** `tests/bug_stb_vorbis_015.c` — would need ~2B samples to trigger; fix prevents overflow at source
- **Fix:** Added `if (total > INT_MAX / 2)` guard before `total *= 2` in both `stb_vorbis_decode_filename` and `stb_vorbis_decode_memory`.

---

| Bug ID | Severity | Class | Status | Notes |
|--------|----------|-------|--------|-------|
| BUG-stb_vorbis-011 | High | UB (left shift negative) | Patched | Cast get8_packet to uint32 before shift at :1594-1595 |
| BUG-stb_vorbis-012 | High | Signed Int Overflow | Patched | len > INT_MAX-1 guard at :3661 and :3688 |
| BUG-stb_vorbis-013 | Medium | Data Corruption (sign error) | Patched | Fixed p_inter*ch - c_inter → p_inter*ch + c_inter at :1899 |
| BUG-stb_vorbis-014 | Medium | Temp Buffer OOB Write | Patched | Added bounds check before classifications[j][i+pcount] write |
| BUG-stb_vorbis-015 | Low | Signed Int Overflow | Patched | Added INT_MAX/2 guard before total *= 2 in decode helpers |
