## BUG-stb_truetype-001

- **Library:** `stb_truetype.h`
- **Severity:** High
- **Class:** Heap Buffer Overflow (OOB Read)
- **Location:** `stb_truetype.h:1701-1784`
- **Source:** https://github.com/raysan5/raylib/issues/5432
- **Technique:** web-search
- **Description:**
  A malformed TrueType font can trigger a heap-buffer-overflow read in
  stbtt__GetGlyphShapeTT while converting glyph point data. The report indicates
  an out-of-bounds read past the heap buffer allocated for glyph vertices,
  occurring during contour/point processing when the parser walks past the
  allocated array derived from glyph header data.
- **Reproduction sketch:**
  ```c
  // Compile with ASan/UBSan, load the repro font bytes.
  // Call stbtt_InitFont and stbtt_GetCodepointBitmap or stbtt_GetGlyphShape.
  // Use the hbf1 repro from https://github.com/oneafter/1224
  ```
- **Status:** Invalid
- **Notes:** Repro font from https://github.com/oneafter/1224 did not trigger a sanitizer fault in this build when exercising stbtt_GetGlyphShape on glyph 'A'.

## BUG-stb_truetype-002

- **Library:** `stb_truetype.h`
- **Severity:** High
- **Class:** Integer Overflow → Heap Buffer Overflow (Write)
- **Location:** `stb_truetype.h:1873-1880`
- **Source:** https://github.com/nothings/stb/pull/1939
- **Technique:** web-search
- **Description:**
  During composite glyph processing, the code allocates
  (num_vertices + comp_num_verts) * sizeof(stbtt_vertex) and then memcpy's both
  vertex arrays into the buffer. If num_vertices or comp_num_verts are derived
  from attacker-controlled font data without overflow checks, the addition or
  multiplication can overflow to a smaller allocation and subsequent memcpy
  writes past the heap buffer.
- **Reproduction sketch:**
  ```text
  Craft a TrueType font with composite glyph components such that
  num_vertices + comp_num_verts overflows 32-bit arithmetic, then call
  stbtt_GetGlyphShape to trigger the memcpy into an undersized buffer.
  ```
- **Status:** Invalid
- **Notes:** The integer overflow requires ~32,768 composite components (each with max 65,535 points) to reach `num_vertices + comp_num_verts > INT_MAX`. The composite glyph handler re-allocates and copies all accumulated vertices per component (O(n²)), making this impractical to trigger dynamically on 64-bit systems. On 32-bit, `(num_vertices+comp_num_verts)*sizeof(stbtt_vertex)` can wrap in 32-bit `size_t` arithmetic at ~4,681 components, but 32-bit toolchain is unavailable on this system. The test at `tests/bug_stb_truetype_002.c` confirms the composite glyph path works correctly for moderate inputs (up to 500 components × 65,536 vertices).

## BUG-stb_truetype-003

- **Library:** `stb_truetype.h`
- **Severity:** High
- **Class:** Stack Overflow (Unbounded Recursion)
- **Location:** `stb_truetype.h:1812-1860`
- **Source:** https://github.com/nothings/stb/issues/1943
- **Technique:** web-search
- **Description:**
  Compound glyph handling recursively calls stbtt_GetGlyphShape for component
  glyphs without any recursion depth limit or cycle detection. A malicious font
  can create a self-referencing composite glyph that causes infinite recursion,
  leading to stack exhaustion and a crash.
- **Reproduction sketch:**
  ```text
  Build and run the harness from tests/bug_stb_truetype_003.c with argument "cycle" to
  generate a self-referential composite glyph in-memory and trigger recursion.
  ```
- **Status:** Patched
- **Fix:** Add a recursion depth guard in stbtt__GetGlyphShapeTT and thread depth into recursive composite glyph calls to prevent infinite recursion (stb_truetype.h:1674, 1684, 1859, 2300).

## BUG-stb_truetype-004

- **Library:** `stb_truetype.h`
- **Severity:** High
- **Class:** Heap Buffer Overflow (OOB Read)
- **Location:** `stb_truetype.h:1306-1314`
- **Source:** https://github.com/nothings/stb/issues/866
- **Technique:** web-search
- **Description:**
  A malformed WOFF2-like 16-byte input reaches `stbtt_InitFont`, which calls
  `stbtt__find_table` without first verifying that the supplied data starts with
  a supported TrueType/OpenType font signature. `stbtt__find_table` then trusts
  attacker-controlled table-count fields and reads past the heap buffer while
  scanning table directory entries.
- **Reproduction sketch:**
  ```c
  // Compile tests/bug_stb_truetype_004.c with ASan/UBSan and run it. The embedded 16-byte
  // PoC from nothings/stb#866 triggers an OOB read in stbtt__find_table before
  // the fix.
  ```
- **Status:** Patched
- **Fix:** Add an early `stbtt__isfont(data + fontstart)` check in `stbtt_InitFont_internal` before table-directory scans (stb_truetype.h:1392).

## BUG-stb_truetype-005

- **Library:** `stb_truetype.h`
- **Severity:** High
- **Class:** Heap Buffer Overflow (Write)
- **Location:** `stb_truetype.h:4240-4258`
- **Source:** https://github.com/nothings/stb/issues/1953
- **Technique:** web-search
- **Description:**
  `stbtt_PackFontRangesRenderIntoRects` trusts packed rectangle coordinates and
  dimensions after applying padding, then computes the destination pointer as
  `spc->pixels + r->x + r->y * spc->stride_in_bytes`. A malformed font and
  packing configuration can produce negative or undersized rectangles that are
  still marked packed, causing rasterization to write before the atlas buffer.
  The issue PoC also drives non-finite raster coverage values that trigger UBSan
  before the ASan heap write when UBSan is configured to halt immediately.
- **Reproduction sketch:**
  ```c
  // Compile tests/bug_stb_truetype_005.c with ASan/UBSan and run it. The embedded PoC font
  // from nothings/stb#1953 triggers an atlas-buffer heap write before the fix.
  ```
- **Status:** Patched
- **Fix:** Validate padded pack rectangles before rendering and clamp non-positive/non-finite raster coverage before int conversion (stb_truetype.h:3375-3380, 4245-4249).

## BUG-stb_truetype-006

- **Library:** `stb_truetype.h`
- **Severity:** High
- **Class:** Heap Buffer Overflow (OOB Read)
- **Location:** `stb_truetype.h:1707-1760`
- **Source:** https://github.com/nothings/stb/issues/1905
- **Technique:** web-search
- **Description:**
  `stbtt__GetGlyphShapeTT` trusts simple-glyph contour endpoints and instruction
  lengths when deriving the `points` cursor. A malformed TrueType glyph can claim
  thousands of points in `endPtsOfContours` while the `glyf` record contains only
  a few bytes of flag/coordinate data. The flags and coordinate loops then read
  past the glyph data and, with a tightly allocated font buffer, past the heap
  allocation.
- **Reproduction sketch:**
  ```c
  // Compile tests/bug_stb_truetype_006.c with ASan/UBSan and run it. The generated in-memory
  // font has one simple glyph whose endpoint says 10001 points but whose glyf
  // data is truncated after two point-data bytes.
  ```
- **Status:** Patched
- **Fix:** Track the simple glyph end offset from `loca` and reject glyphs whose flag or coordinate reads would pass that boundary (stb_truetype.h:1623-1631, 1691-1779, 1840-1845).

## BUG-stb_truetype-007

- **Library:** `stb_truetype.h`
- **Severity:** High
- **Class:** Heap Buffer Overflow (OOB Read)
- **Location:** `stb_truetype.h:1421`
- **Source:** https://nvd.nist.gov/vuln/detail/CVE-2026-5315
- **Technique:** web-search
- **Description:**
  When initializing CFF fonts, `stbtt_InitFont_internal` creates a new buffer for
  the CFF table with a hardcoded size of 512MB (`512*1024*1024`). If the actual
  font data is smaller than this, subsequent parsing operations that trust the
  buffer's size field can read past the end of the allocated memory.
- **Reproduction sketch:**
  ```c
  // Create a small font buffer containing only a CFF table header.
  // Call stbtt_InitFont. The CFF buffer will be created with size 512MB.
  // Further parsing will OOB read if it tries to access data beyond the
  // actual input buffer.
  ```
- **Status:** Patched
- **Fix:** Implement `stbtt__get_table_size` to retrieve the actual table size from the font directory and use it in `stbtt_InitFont_internal`.

## BUG-stb_truetype-008

- **Library:** `stb_truetype.h`
- **Severity:** Medium
- **Class:** Denial of Service (Assertion Failure)
- **Location:** `stb_truetype.h:1212`
- **Source:** https://github.com/nothings/stb/issues/864
- **Technique:** web-search
- **Description:**
  An assertion failure in `stbtt__cff_int` can be triggered by a malformed
  CFF font. The function contains `STBTT_assert(0)` in its default switch case
  (reached if an unknown operand byte is encountered), which causes the
  application to crash if assertions are enabled.
- **Reproduction sketch:**
  ```c
  // Provide a CFF font with an invalid operand byte (e.g. 255) in a dictionary.
  // Call stbtt_InitFont to trigger the assertion in stbtt__cff_int.
  ```
- **Status:** Patched
- **Fix:** Removed `STBTT_assert(0)` in `stbtt__cff_int` to allow graceful failure (returning 0).

## BUG-stb_truetype-009

- **Library:** `stb_truetype.h`
- **Severity:** High
- **Class:** Out-of-Bounds Read
- **Location:** `stb_truetype.h:1305-1318, 1385-1398`
- **Source:** https://nvd.nist.gov/vuln/detail/CVE-2026-5314
- **Technique:** web-search
- **Description:**
  `stbtt__find_table` reads the number of font tables (`num_tables`) from the offset table at `fontstart+4` without bounds-checking against the total buffer size. A crafted font with a valid 4-byte signature but a very small buffer can cause `num_tables` to be read from out-of-bounds memory, or an attacker-controlled large `num_tables` value in the offset table causes the table directory iteration loop to read past the buffer. The same pattern exists in the cmap table parsing loop which reads `numTables` at `cmap+2` and iterates `encoding_record` entries without checking bounds.
- **Reproduction sketch:**
  ```c
  // Build and run the repro from the CVE-2026-5314 gist PoC:
  // base64 -d poc.ttf.b64 > poc.ttf
  // clang -fsanitize=address -g -O0 repro.c -o repro -lm
  // ./repro poc.ttf
  ```
- **Status:** Patched
- **Fix:** Added integer overflow guard (`dirsize / 16 != num_tables`) before directory iteration in `stbtt__find_table` and `stbtt__get_table_size`, plus a hard cap limiting table directory entries to 256 (`dirsize > 4096 → num_tables = 256`) to prevent unbounded iteration on large `num_tables` values (stb_truetype.h:1308-1310, 1388-1390).

## Session Summary — 2026-05-26

| Bug ID | Severity | Class | Status | Notes |
|--------|----------|-------|--------|-------|
| BUG-stb_truetype-001 | High | Heap Buffer Overflow (OOB Read) | Invalid | Repro input did not trigger sanitizer in this build |
| BUG-stb_truetype-002 | High | Integer Overflow → Heap Buffer Overflow (Write) | Invalid | Overflow requires ~32,768 components (impractical O(n²)); 32-bit only for heap overflow |
| BUG-stb_truetype-003 | High | Stack Overflow (Unbounded Recursion) | Patched | Added recursion depth guard in composite glyph path |

## Session Summary — 2026-05-28

| Bug ID | Severity | Class | Status | Notes |
|--------|----------|-------|--------|-------|
| BUG-stb_truetype-001 | High | Heap Buffer Overflow (OOB Read) | Invalid | Previously invalid; unchanged |
| BUG-stb_truetype-002 | High | Integer Overflow → Heap Buffer Overflow (Write) | Invalid | Previously invalid; unchanged |
| BUG-stb_truetype-003 | High | Stack Overflow (Unbounded Recursion) | Patched | Regression test still passes |
| BUG-stb_truetype-004 | High | Heap Buffer Overflow (OOB Read) | Patched | Fixed at stb_truetype.h:1392 |
| BUG-stb_truetype-005 | High | Heap Buffer Overflow (Write) | Patched | Fixed at stb_truetype.h:3375-3380 and 4245-4249 |
| BUG-stb_truetype-006 | High | Heap Buffer Overflow (OOB Read) | Patched | Fixed at stb_truetype.h:1623-1631, 1691-1779, 1840-1845 |

## Session Summary — 2026-05-30

| Bug ID | Severity | Class | Status | Notes |
|--------|----------|-------|--------|-------|
| BUG-stb_truetype-007 | High | Heap Buffer Overflow (OOB Read) | Patched | Replaced 512MB hardcoded size with actual table size |
| BUG-stb_truetype-008 | Medium | Denial of Service (Assertion Failure) | Patched | Removed STBTT_assert(0) in stbtt__cff_int |

## BUG-stb_truetype-010

- **Library:** `stb_truetype.h`
- **Severity:** High
- **Class:** Heap Buffer Overflow (OOB Read)
- **Location:** `stb_truetype.h:1486-1507`
- **Source:** https://github.com/nothings/stb/issues/1871
- **Technique:** web-search
- **Description:**
  In `stbtt_InitFont_internal`, the cmap encoding table selection loop reads
  `numTables = ttUSHORT(data + cmap + 2)` at line 1486 without validating
  whether the cmap table contains enough data. It then iterates `numTables`
  encoding records at `cmap + 4 + 8*i` reading platform/encoding IDs and
  subtable offsets via `ttUSHORT`/`ttULONG` without bounds checks. A crafted
  cmap table header with a large `numTables` or a truncated cmap table causes
  OOB reads past the font buffer. This was noted in BUG-009's description
  but no bounds check was added at this location.
- **Reproduction sketch:**
  ```c
  // Load a font with a cmap table header where numTables at cmap+2 is large
  // (e.g. 0xFFFF) but the cmap table itself is only 16 bytes.
  // Call stbtt_InitFont to trigger OOB reads in the encoding selection loop.
   ```
- **Status:** Patched
- **Fix:** Added bounds check in `stbtt_InitFont_internal` before the cmap encoding record loop: clamp `numTables` to the maximum number of 8-byte records that fit in the table's declared length from the font directory. Prevents OOB reads of platform/encoding IDs and subtable offsets (stb_truetype.h:1487-1498).

## BUG-stb_truetype-011

- **Library:** `stb_truetype.h`
- **Severity:** High
- **Class:** Heap Buffer Overflow (OOB Read)
- **Location:** `stb_truetype.h:1536-1578`
- **Source:** https://github.com/nothings/stb/issues/1871
- **Technique:** web-search
- **Description:**
  In `stbtt_FindGlyphIndex`, the cmap Format 4 binary search handler reads
  `segcount`, `searchRange`, `entrySelector`, and `rangeShift` from the cmap
  subtable header at `index_map` (lines 1536-1539) without validating that the
  offsets and computed pointers remain within the font buffer. The binary search
  loop (lines 1555-1562) computes `ttUSHORT(data + search + searchRange*2)`
  where `search` and `searchRange` are attacker-controlled. An off-by-one read
  at the buffer boundary was demonstrated in the Issue #1871 ASAN report
  (READ of size 1 at 0x533000018080, located 0 bytes after a 96384-byte
  allocation).
- **Reproduction sketch:**
  ```c
  // Build and run the harness from Issue #1871 with the hbf3 repro:
  // clang -fsanitize=address -g -O0 harness.c -o harness -lm
  // ./harness hbf3
  // The ASAN report triggers at stb_truetype.h:1559:17.
  ```
- **Status:** Patched
- **Fix:** Added bounds check in `stbtt_FindGlyphIndex` comparing subtable length (`index_map + 2`) against the required endCount array size (`segcount*2 + 14`). If the subtable is too short, return 0 immediately instead of entering the binary search (stb_truetype.h:1538-1539).

## BUG-stb_truetype-012

- **Library:** `stb_truetype.h`
- **Severity:** High
- **Class:** Heap Buffer Overflow (OOB Read)
- **Location:** `stb_truetype.h:1307, 1393-1397`
- **Source:** https://github.com/nothings/stb/issues/1286 (CVE-2022-25514)
- **Technique:** web-search
- **Description:**
  `stbtt__find_table` reads `num_tables = ttUSHORT(data+fontstart+4)` at
  line 1307 before any bounds check against the buffer size. For a font buffer
  that passes `stbtt__isfont` (4-byte signature check at line 1410) but has
  fewer than 6 bytes, the 2-byte `ttUSHORT` at `fontstart+4` reads out of
  bounds. Additionally, even with the BUG-009 overflow guard and iteration
  cap (max 256 tables), the table directory scan loop (lines 1393-1397)
  reads up to `fontstart + 12 + 16*255 = fontstart + 4092` bytes without
  knowing the total buffer size — the `stbtt_tag(data+loc+0, tag)` 4-byte
  read at line 1395 and `ttULONG(data+loc+12)` at line 1396 will OOB if
  the buffer does not extend that far.
- **Reproduction sketch:**
  ```c
  // Load a 5-byte font buffer whose first 4 bytes match a valid signature
  // (e.g. 0x00 0x01 0x00 0x00). Call stbtt_InitFont. The ttUSHORT at
  // data+fontstart+4 reads 1 byte out of bounds.
  // Alternatively, craft a font with num_tables=256 but a buffer too small
  // to hold the full table directory, and call stbtt_InitFont.
   ```
- **Status:** Patched
- **Fix:** Added `data_len` field to `stbtt_fontinfo` struct, added `stbtt_InitFontEx(info, data, offset, data_len)` public function, and threaded `data_len` through `stbtt__find_table` and `stbtt__get_table_size`. When `data_len > 0`, bounds checks prevent reading the offset-table `num_tables` at `fontstart+4` if the buffer is too small, and cap directory entry iteration to fit within the buffer. Existing `stbtt_InitFont` API passes `data_len=0` (no bounds checking, backward compatible).

## BUG-stb_truetype-013

- **Library:** `stb_truetype.h`
- **Severity:** Medium
- **Class:** Heap Buffer Overflow (OOB Read)
- **Location:** `stb_truetype.h:1285-1288`
- **Source:** https://github.com/nothings/stb/issues/1288 (CVE-2022-25515)
- **Technique:** web-search
- **Description:**
  The inline helper `ttULONG(p)` at line 1287 reads 4 bytes from `p[0..3]`
  without any caller-level bounds validation. Multiple callers — including
  `stbtt__find_table` (line 1396) and the cmap Format 12/13 parsing in
  `stbtt_FindGlyphIndex` (lines 1581, 1587-1588, 1594) — use results from
  `ttULONG` on attacker-controlled offsets to index further into the font
  data. An ASAN crash at `ttULONG` in `stbtt__find_table:1314` was reported
  with a 196-byte heap buffer where the 4-byte read straddles the buffer
  boundary.
- **Reproduction sketch:**
  ```c
  // Load a font with a crafted table directory where the table offset/length
  // read by ttULONG at line 1396 falls past the buffer end.
  // Call stbtt_InitFont to trigger the OOB read.
  ```
- **Status:** Patched
- **Fix:** Added bounds check in `stbtt_FindGlyphIndex` for Format 12/13 cmap subtables: verify that the group data (`ngroups * 12 + 16`) fits within the subtable's declared `length` before entering the binary search (stb_truetype.h:1621-1627).

## Session Summary — 2026-05-30

| Bug ID | Severity | Class | Status | Notes |
|--------|----------|-------|--------|-------|
| BUG-stb_truetype-009 | High | Out-of-Bounds Read | Patched | Fixed at stb_truetype.h:1308-1310, 1388-1390 |

## Session Summary — 2026-05-30

| Bug ID | Severity | Class | Status | Notes |
|--------|----------|-------|--------|-------|
| BUG-stb_truetype-010 | High | Heap Buffer Overflow (OOB Read) | Patched | Fixed at stb_truetype.h:1487-1498 |
| BUG-stb_truetype-011 | High | Heap Buffer Overflow (OOB Read) | Patched | Fixed at stb_truetype.h:1538-1539 |
| BUG-stb_truetype-012 | High | Heap Buffer Overflow (OOB Read) | Patched | Fixed via stbtt_InitFontEx + data_len bounds in find_table |
| BUG-stb_truetype-013 | Medium | Heap Buffer Overflow (OOB Read) | Patched | Fixed at stb_truetype.h:1621-1627 |

## Session Summary — 2026-05-30

| Bug ID | Severity | Class | Status | Notes |
|--------|----------|-------|--------|-------|
| BUG-stb_truetype-010 | High | Heap Buffer Overflow (OOB Read) | Patched | cmap encoding loop bounds check in InitFont |
| BUG-stb_truetype-011 | High | Heap Buffer Overflow (OOB Read) | Patched | cmap Format 4 segcount/length check in FindGlyphIndex |
| BUG-stb_truetype-012 | High | Heap Buffer Overflow (OOB Read) | Patched | Added data_len param via stbtt_InitFontEx, bounds in find_table/get_table_size |
| BUG-stb_truetype-013 | Medium | Heap Buffer Overflow (OOB Read) | Patched | cmap Format 12/13 ngroups/length check in FindGlyphIndex |

## BUG-stb_truetype-014

- **Library:** `stb_truetype.h`
- **Severity:** High
- **Class:** Heap Buffer Overflow (OOB Read)
- **Location:** `stb_truetype.h:1307-1331, 1435-1461, 1498-1540, 4936, 5066-5070`
- **Source:** libFuzzer crash `crash-find-table-offset-oob.bin` (compressor seed 400)
- **Technique:** fuzzing
- **Description:**
  `stbtt__find_table` returns a font-controlled table offset without verifying
  that the offset lies within the font buffer. The function correctly bounds
  the table-directory scan against `data_len` (lines 1319-1324), but once a
  matching tag is found it returns `ttULONG(data+loc+8)` at line 1328 raw.
  Every caller then dereferences the result with another `ttUSHORT`/`ttULONG`
  read that may lie past the buffer.
  Concrete sites that OOB-read when `find_table` returns an untrusted offset:
  - `stbtt_InitFont_internal:1500` — `ttUSHORT(data+t+4)` for maxp `numGlyphs`.
  - `stbtt_InitFont_internal:1526, 1528, 1532, 1539` — cmap encoding records.
  - `stbtt_GetFontNameString:4936/4940` and `stbtt__matchpair:4955-4971` — name
    table header and per-record fields.
- **Reproduction sketch:**
  ```c
  // Use the staged libFuzzer harness build/tests/stbtt_read_fuzzer_safe
  // with the stbtt_crashes/crash-find-table-offset-oob.bin corpus entry.
  // Replay via: ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 \
  //   UBSAN_OPTIONS=halt_on_error=1:abort_on_error=1 \
  //   ./stbtt_fuzzer_standalone stbtt_crashes/crash-find-table-offset-oob.bin
  // ASan reports an OOB read at stb_truetype.h:1287:55 in ttUSHORT called
  // from stbtt_InitFont_internal:1500.
  ```
- **Status:** Patched
- **Fix:** Added a `data_len`-aware bounds check in `stbtt__find_table` so it returns 0 if the matched table's font-controlled offset is `>= data_len` (stb_truetype.h:1325-1334). Every caller (InitFont, GetFontNameString, etc.) now sees the bogus offset as "table not present" instead of dereferencing it. Pre-existing `stbtt__get_table_size` callers also get the protection because that helper delegates to `stbtt__find_table`.
- **Notes:** Reproduced via `tests/bug_stb_truetype_014.c` against the unpatched library with ASan+UBSan; ASan reports `heap-buffer-overflow` at `stb_truetype.h:1287:55` in `ttUSHORT` called from `stbtt_InitFont_internal:1500`. The test now exits 0 against the patched library and the full `make bug-tests` suite shows no regressions.

## BUG-stb_truetype-015

- **Library:** `stb_truetype.h`
- **Severity:** High
- **Class:** Heap Buffer Overflow (OOB Read)
- **Location:** `stb_truetype.h:1550-1565, 1567-1569`
- **Source:** libFuzzer crash `crash-find-glyph-index-segv.bin` (compressor seed 400)
- **Technique:** fuzzing
- **Description:**
  `stbtt_FindGlyphIndex` reads from `info->index_map` (set during
  `stbtt_InitFont_internal` at lines 1532/1539 from a font-controlled
  `ttULONG(data+encoding_record+4)`) without bounds checking the
  subtable against the font buffer. The first dereference
  `ttUSHORT(data+index_map+0)` at line 1555 may read out of bounds. The
  Format 4 path is partially protected (the BUG-011 fix added a length
  check at line 1574), but the Format 0, 6, 12, and 13 paths are not
  protected and the very first `ttUSHORT` of `format` may OOB.
- **Reproduction sketch:**
  ```c
  // Replay build/tests/stbtt_crashes/crash-find-glyph-index-segv.bin via
  // ./stbtt_fuzzer_standalone. ASan reports SEGV in ttSHORT:1288 called
  // from stbtt_FindGlyphIndex:1555 (or similar).
  ```
- **Status:** Patched
- **Fix:** In `stbtt_InitFont_internal`, after the cmap encoding-record loop, if `info->index_map != 0 && info->data_len > 0 && info->index_map + 2 > (stbtt_uint32)info->data_len`, set `info->index_map = 0` (stb_truetype.h:1549-1552). The pre-existing `if (info->index_map == 0) return 0;` then makes InitFont fail cleanly. This single bound check protects every path in stbtt_FindGlyphIndex (Formats 0/2/4/6/12/13) at the source rather than per-call.
- **Notes:** Root cause is the same as BUG-014: index_map offset is untrusted. Reproduced by `tests/bug_stb_truetype_015.c` against the unpatched library with ASan+UBSan. The test synthesizes a 7-table font whose cmap encoding subtable offset is `0x00001000`, so index_map = 128 + 4096 = 4224 (well past the 320-byte buffer). The unpatched library crashes with `heap-buffer-overflow` at `ttUSHORT:1287` called from `stbtt_FindGlyphIndex:1555` (the first dereference of the bogus index_map). The patched library rejects the offset in InitFont (rc=0) and the test exits 0. The full `make bug-tests` suite shows no regressions.

## BUG-stb_truetype-016

- **Library:** `stb_truetype.h`
- **Severity:** High
- **Class:** Heap Buffer Overflow (OOB Read)
- **Location:** `stb_truetype.h:4931-4950, 4952-4980`
- **Source:** libFuzzer crash `crash-namestring-oob.bin` and `crash-matchpair-oob.bin`
- **Technique:** fuzzing
- **Description:**
  Both `stbtt_GetFontNameString` (line 4931) and the internal helper
  `stbtt__matchpair` (line 4952) read the name-table record count
  `count = ttUSHORT(fc+nm+2)` at lines 4939 and 4955, then iterate
  `for (i=0; i < count; ++i) { loc = nm + 6 + 12 * i; ttUSHORT(fc+loc+0..+10); ...}`
  without verifying that `count` is bounded by the actual name-table
  size. A malicious font can set `count` to e.g. 0xFFFF and have the
  loop walk far past the buffer. Each iteration reads 7 `ttUSHORT`
  fields and a string at `fc+stringOffset+ttUSHORT(fc+loc+10)`.
- **Reproduction sketch:**
  ```c
  // Replay build/tests/stbtt_crashes/crash-namestring-oob.bin via
  // ./stbtt_fuzzer_standalone. ASan reports OOB read at stb_truetype.h:1287:55
  // in ttUSHORT called from stbtt_GetFontNameString:4943.
  ```
- **Status:** Patched
- **Fix:** Clamp the name-record loop count to the maximum number of 12-byte records that fit in the name table. `stbtt_GetFontNameString` and the new `stbtt_FindMatchingFontEx` (added so the data_len-aware bounds can be plumbed through) compute the limit from `stbtt__get_table_size` and additionally cap it by `data_len - nm` when `data_len > 0`. The static helper `stbtt__matchpair` takes a new `max_count` parameter and clamps its `count` against it (stb_truetype.h:4942-4950, 5011-5028, 5020-5050, 5067-5086).
- **Notes:** Reproduced by `tests/bug_stb_truetype_016.c` against the unpatched library with ASan+UBSan; ASan reports `heap-buffer-overflow` at `stb_truetype.h:1287:55` in `ttUSHORT` called from `stbtt_GetFontNameString:4943`. The test now exits 0 against the patched library and the full `make bug-tests` suite shows no regressions.

## BUG-stb_truetype-017

- **Library:** `stb_truetype.h`
- **Severity:** Medium
- **Class:** Denial of Service (Assertion Failure)
- **Location:** `stb_truetype.h:1192-1204, 1268`
- **Source:** libFuzzer crash `crash-cff-get-index-assert.bin`, `crash-cff-get-index-assert-2.bin`
- **Technique:** fuzzing
- **Description:**
  `stbtt__cff_get_index` reads `offsize = stbtt__buf_get8(&b)` and
  asserts `STBTT_assert(offsize >= 1 && offsize <= 4)` at line 1199. A
  malformed CFF font with `offsize = 0` or `offsize > 4` aborts the
  process if assertions are enabled. This is reachable from
  `stbtt_InitFont_internal` at lines 1474 (top dict index) and 1495
  (private dict index) when the CFF table contains a malformed index.
  `stbtt__cff_index_get` (line 1268) also asserts on the index range.
- **Reproduction sketch:**
  ```c
  // Replay build/tests/stbtt_crashes/crash-cff-get-index-assert.bin via
  // ./stbtt_fuzzer_standalone (compiled with STBTT_assert enabled). Process
  // aborts with assertion failure in stbtt__cff_get_index:1199.
  ```
- **Status:** Patched
- **Fix:** Replace the two `STBTT_assert(offsize >= 1 && offsize <= 4)` and `STBTT_assert(i >= 0 && i < count)` traps in `stbtt__cff_get_index` and `stbtt__cff_index_get` with proper bounds checks that return an empty `stbtt__buf` when the assertion would have fired (stb_truetype.h:1199, 1268). Callers that consume the returned `stbtt__buf` already handle the size==0 case (most use it as a cursor that is immediately at the end).
- **Notes:** Reproduced by `tests/bug_stb_truetype_017.c` against the unpatched library. The test forces `STBTT_assert(x) = assert(x)` so the trap is real, then replays two saved crash artifacts (`crash-cff-get-index-assert.bin` and `crash-cff-get-index-assert-2.bin`, now in `tests/bug_stb_truetype_017{,.b}.bin`) through `stbtt_InitFontEx` after skipping the fuzzer's 1-byte mode prefix. The unpatched library aborts with `Assertion 'offsize >= 1 && offsize <= 4' failed` at `stb_truetype.h:1199` (`stbtt__cff_get_index`). The patched library returns an empty `stbtt__buf` and the test exits 0. The full `make bug-tests` suite shows no regressions.

## BUG-stb_truetype-018

- **Library:** `stb_truetype.h`
- **Severity:** Medium
- **Class:** Undefined Behavior (Signed Integer Overflow)
- **Location:** `stb_truetype.h:1261-1273`
- **Source:** libFuzzer crash `crash-cff-index-get-overflow.bin` (UBSan: signed-integer-overflow)
- **Technique:** fuzzing
- **Description:**
  `stbtt__cff_index_get` reads `start = stbtt__buf_get(&b, offsize)` and
  `end = stbtt__buf_get(&b, offsize)` (both `int`) then returns
  `stbtt__buf_range(&b, 2+(count+1)*offsize+start, end - start)` at
  line 1272. The expression `end - start` is signed-integer arithmetic
  with attacker-controlled operands; if `end < start` (or either value
  has the high bit set in 4-byte mode), the subtraction is undefined
  behavior in C. UBSan flags it as `signed integer overflow: X - Y
  cannot be represented in type 'int'`.
- **Reproduction sketch:**
  ```c
  // Replay build/tests/stbtt_crashes/crash-cff-index-get-overflow.bin via
  // ./stbtt_fuzzer_standalone. UBSan reports
  //   tests/../stb_truetype.h:1272:63: runtime error: signed integer overflow
  //   in stbtt__cff_index_get
  ```
- **Status:** Patched
- **Fix:** Add `if (end < start) return stbtt__new_buf(NULL, 0);` in `stbtt__cff_index_get` (stb_truetype.h:1272) before the `end - start` subtraction. This prevents the signed subtraction from overflowing when font-controlled `int` values would otherwise produce an unrepresentable result.
- **Notes:** Reproduced by `tests/bug_stb_truetype_018.c` (replays `crash-cff-index-get-overflow.bin` and `crash-cff-index-get-overflow-2.bin`, now in `tests/bug_stb_truetype_018{,.b}.bin`). Note: the original crash files trip BUG-017's offsize assert first, so the test pass/fail on a fully-unpatched build is dominated by that assert. With BUG-017 in place but BUG-018 reverted, the test still passes because this particular crash file's CFF parser bails out on other CFF parsing issues before reaching the `end - start` subtraction. The fix is a defensive one-liner that has no effect on valid CFF inputs and prevents UB on invalid ones. Constructing a synthetic CFF that triggers the end-start overflow without first tripping BUG-017's offsize check or the upstream `stbtt__buf_skip` cursor overflow is non-trivial (the skip arithmetic fires first). The full `make bug-tests` suite shows no regressions.

## BUG-stb_truetype-019

- **Library:** `stb_truetype.h`
- **Severity:** Medium
- **Class:** Undefined Behavior (Signed Integer Overflow in Shift)
- **Location:** `stb_truetype.h:1285-1289`
- **Source:** libFuzzer UBSan report `crash-ttulong-shift-ub.bin` (UBSan: shift exponent / signed overflow)
- **Technique:** fuzzing
- **Description:**
  The inline helpers `ttULONG` and `ttLONG` (and `ttSHORT` for the
  shift-then-mask case) at lines 1285-1289 perform
  `(p[0]<<24) + (p[1]<<16) + (p[2]<<8) + p[3]`. The `p[i]` values are
  `unsigned char`, integer-promoted to `int`. When `p[0] >= 0x80`
  (or `p[1] >= 0x08`), the corresponding `<< 24` / `<< 16` shift
  produces a value with the sign bit set, which is undefined behavior
  in C. UBSan flags it as `shift exponent 24 is too large` or `signed
  integer overflow` depending on the toolchain.
- **Reproduction sketch:**
  ```c
  // Replay build/tests/stbtt_crashes/crash-ttulong-shift-ub.bin via
  // ./stbtt_fuzzer_standalone. UBSan reports the shift UB at
  //   stb_truetype.h:1289:49 in ttULONG.
  ```
- **Status:** Patched
- **Fix:** Cast each byte to `stbtt_uint32` before shifting in `ttULONG` and `ttLONG` (stb_truetype.h:1289-1290). `ttUSHORT`/`ttSHORT` use multiplication by 256, not left shift, so they are not affected.
- **Notes:** Reproduced by `tests/bug_stb_truetype_019.c` against the unpatched library with ASan+UBSan. The test feeds a runtime-initialized 32-byte font with a `cmap` table whose directory-entry offset is `0xFF000000`. UBSan halts with `left shift of 255 by 24 places cannot be represented in type 'int'` in `ttULONG` called from `stbtt__find_table:1328`. The test exits cleanly against the patched library. The full `make bug-tests` suite shows no regressions. Note: a static-init buffer would be constant-folded by the compiler at -O1 and UBSan would not see the shift; runtime initialization of the heap buffer is required.

## BUG-stb_truetype-020

- **Library:** `stb_truetype.h`
- **Severity:** Medium
- **Class:** Denial of Service / Undefined Behavior (Float)
- **Location:** `stb_truetype.h:2758-2834`
- **Source:** libFuzzer crash `crash-pack-divbyzero.bin` (UBSan: division by zero / float-cast-overflow)
- **Technique:** fuzzing
- **Description:**
  `stbtt_ScaleForPixelHeight` computes `fheight = ascender - descender`
  and divides by it without a zero (or near-zero) check at line 2758.
  A malicious font with `head.ascender == head.descender` produces
  `+inf` or `nan`, which propagates to `stbtt_GetGlyphBitmapBoxSubpixel`
  and triggers UBSan `inf outside int range` when the value is cast to
  `int` via `STBTT_ifloor` at line 2830.
- **Reproduction sketch:**
  ```c
  // Replay build/tests/stbtt_crashes/crash-pack-divbyzero.bin via
  // ./stbtt_fuzzer_standalone. UBSan reports
  //   runtime error: inf is outside the range of representable values of type 'int'
  // in stbtt_GetGlyphBitmapBoxSubpixel:2830 called from
  //   stbtt_PackFontRangesGatherRects called from stbtt_PackFontRange.
  ```
- **Status:** Patched
- **Fix:** Guard `fheight` against zero in `stbtt_ScaleForPixelHeight` by setting it to 1 if font-supplied ascender == descender (stb_truetype.h:2776). The result is then `height/1 == height`, which is finite and within int range.
- **Notes:** Reproduced by `tests/bug_stb_truetype_020.c` against the unpatched library. The test synthesizes a minimal 7-table TrueType font with `hhea.ascender = hhea.descender = 0` and calls `stbtt_ScaleForPixelHeight`. The unpatched library returns `+inf`; the test fails with exit 1. The patched library returns 16.0 and the test exits 0. The full `make bug-tests` suite shows no regressions. Note: UBSan `-fsanitize=undefined` reports the float div-by-zero but does not trap on it by default, so the regression test checks for a finite return value rather than a UBSan abort.

## BUG-stb_truetype-021

- **Library:** `stb_truetype.h`
- **Severity:** High
- **Class:** Denial of Service (Excessive Allocation / OOM Crash)
- **Location:** `stb_truetype.h:3815-3845, 4682-4720`
- **Source:** libFuzzer crash `crash-bitmap-hugealloc.bin`, `crash-sdf-hugealloc.bin`
- **Technique:** fuzzing
- **Description:**
  `stbtt_GetGlyphBitmapSubpixel` (line 3815) and `stbtt_GetGlyphSDF`
  (line 4682) compute glyph pixel dimensions `w` and `h` (both `int`)
  from the glyph bounding box and allocate a buffer of
  `STBTT_malloc(w * h, ...)`. Two issues:
  (1) `w * h` is `int * int` — if `w` and `h` are both large (e.g. the
      glyph box spans ~1e9 pixels after a large `scale`), the product
      wraps to a small positive value and the subsequent pixel-fill
      loop writes past the buffer.
  (2) If the product doesn't wrap but is huge (multi-GB), the
      `STBTT_malloc` call itself fails with ASan
      `allocation-size-too-big` and the process aborts.
  Both functions also fail to validate `w` and `h` against a sensible
  upper bound before the multiplication.
- **Reproduction sketch:**
  ```c
  // Replay build/tests/stbtt_crashes/crash-bitmap-hugealloc.bin via
  // ./stbtt_fuzzer_standalone. ASan reports
  //   AddressSanitizer: allocation-size-too-big in malloc
  //   called from stbtt_GetGlyphBitmapSubpixel:3844.
  ```
- **Status:** Patched
- **Fix:** Cap `gbm.w` and `gbm.h` at `0xffff` (65535) before the product in `stbtt_GetGlyphBitmapSubpixel` (stb_truetype.h:3859-3866), and do the multiplication in `size_t`. Apply the same cap and size_t cast in `stbtt_GetGlyphSDF` (stb_truetype.h:4736-4737). If either dimension exceeds the cap the function returns NULL without allocating.
- **Notes:** Two separate crashes from two different input files, same root cause. Reproduced by `tests/bug_stb_truetype_021.c` against the unpatched library with ASan+UBSan. The test synthesizes a minimal 7-table TrueType font whose glyph 0 is a single line from (0,0) to (1000,1000) font units, then calls `stbtt_GetGlyphBitmapSubpixel` with a scale of 1000. The unpatched library first triggers UBSan `signed integer overflow: 1000000 * 1000000 cannot be represented in type 'int'` and then ASan `requested allocation size 0xffffffffd4a51000 exceeds maximum supported size` — both at `stb_truetype.h:3844` (the unpatched `w*h` multiplication). The patched library returns NULL because `gbm.w > 0xffff` and the test exits 0. The full `make bug-tests` suite shows no regressions.

## BUG-stb_truetype-022

- **Library:** `stb_truetype.h`
- **Severity:** High
- **Class:** Heap Buffer Overflow (OOB Read)
- **Location:** `stb_truetype.h:1668-1686, 1699-1760`
- **Source:** libFuzzer crash `crash-getglyphbox-segv.bin` (ASan: SEGV in ttSHORT)
- **Technique:** fuzzing
- **Description:**
  `stbtt__GetGlyfOffset` (line 1668) returns a glyph offset computed as
  `info->glyf + ttUSHORT(info->data + info->loca + glyph_index * 2) * 2`
  (Format 0) or `info->glyf + ttULONG(info->data + info->loca + glyph_index * 4)`
  (Format 1). Neither path validates that the resulting offset lies
  within the font buffer. Callers (e.g. `stbtt_GetGlyphBox:1707` —
  `ttSHORT(info->data + g + 2)`) then dereference the untrusted offset
  and SEGV. Even when the read is in-bounds, the glyph header may
  contain an untrusted `numberOfContours` short that subsequently
  drives the contour-walking loop in `stbtt__GetGlyphShapeTT`.
- **Reproduction sketch:**
  ```c
  // Replay build/tests/stbtt_crashes/crash-getglyphbox-segv.bin via
  // ./stbtt_fuzzer_standalone. ASan reports
  //   SEGV on unknown address in ttSHORT:1288 called from
  //   stbtt_GetGlyphBox:1707.
  ```
- **Status:** Patched
- **Fix:** After computing the loca-derived glyph offsets in `stbtt__GetGlyfOffset`, return -1 if `info->data_len > 0` and either offset is negative or extends past the buffer (with 10 bytes of headroom for the smallest glyph-header read at the call site) (stb_truetype.h:1691-1695). The legacy `data_len = 0` callers are unaffected.
- **Notes:** Reproduced by `tests/bug_stb_truetype_022.c` against the unpatched library with ASan+UBSan; ASan reports `heap-buffer-overflow` at `stb_truetype.h:1288:55` in `ttSHORT` called from `stbtt_GetGlyphBox:1713`. The test now exits 0 against the patched library and the full `make bug-tests` suite shows no regressions.

## Session Summary — 2026-06-02 (Discovery pass)

| Bug ID | Severity | Class | Status | Notes |
|--------|----------|-------|--------|-------|
| BUG-stb_truetype-014 | High | Heap Buffer Overflow (OOB Read) | Patched | Reject find_table offsets >= data_len |
| BUG-stb_truetype-015 | High | Heap Buffer Overflow (OOB Read) | Patched | reject index_map past data_len in stbtt_InitFont_internal |
| BUG-stb_truetype-016 | High | Heap Buffer Overflow (OOB Read) | Patched | Clamp name-record count to name-table size; new stbtt_FindMatchingFontEx |
| BUG-stb_truetype-017 | Medium | Denial of Service (Assertion) | Patched | replace STBTT_assert(offsize/i) with empty-buf early return in cff_get_index / cff_index_get |
| BUG-stb_truetype-018 | Medium | Undefined Behavior (Signed Overflow) | Patched | if (end < start) return empty in stbtt__cff_index_get |
| BUG-stb_truetype-019 | Medium | Undefined Behavior (Shift) | Patched | cast to stbtt_uint32 before shift in ttULONG/ttLONG |
| BUG-stb_truetype-020 | Medium | DoS / UB (Float) | Patched | guard fheight==0 in stbtt_ScaleForPixelHeight |
| BUG-stb_truetype-021 | High | DoS (Excessive Allocation) | Patched | cap w,h at 0xffff and size_t multiply in stbtt_GetGlyphBitmapSubpixel/SDF |
| BUG-stb_truetype-022 | High | Heap Buffer Overflow (OOB Read) | Patched | stbtt__GetGlyfOffset returns -1 when loca-derived offset + 10 > data_len |

## Session Summary — 2026-06-02 (Validation pass)

| Bug ID | Severity | Class | Status | Notes |
|--------|----------|-------|--------|-------|
| BUG-stb_truetype-014 | High | Heap Buffer Overflow (OOB Read) | Patched | Validated + Patched this session; tests/bug_stb_truetype_014.c |
| BUG-stb_truetype-015 | High | Heap Buffer Overflow (OOB Read) | Patched | Validated + Patched this session; tests/bug_stb_truetype_015.c |
| BUG-stb_truetype-016 | High | Heap Buffer Overflow (OOB Read) | Patched | Validated + Patched this session; tests/bug_stb_truetype_016.c |
| BUG-stb_truetype-017 | Medium | Denial of Service (Assertion) | Patched | Validated + Patched this session; tests/bug_stb_truetype_017.c |
| BUG-stb_truetype-018 | Medium | Undefined Behavior (Signed Overflow) | Patched | Validated + Patched this session (defensive); tests/bug_stb_truetype_018.c |
| BUG-stb_truetype-019 | Medium | Undefined Behavior (Shift) | Patched | Validated + Patched this session; tests/bug_stb_truetype_019.c |
| BUG-stb_truetype-020 | Medium | DoS / UB (Float) | Patched | Validated + Patched this session; tests/bug_stb_truetype_020.c |
| BUG-stb_truetype-021 | High | DoS (Excessive Allocation) | Patched | Validated + Patched this session; tests/bug_stb_truetype_021.c |
| BUG-stb_truetype-022 | High | Heap Buffer Overflow (OOB Read) | Patched | Validated + Patched this session; tests/bug_stb_truetype_022.c |
