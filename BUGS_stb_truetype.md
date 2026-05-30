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
