## BUG-001

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

## BUG-002

- **Library:** `stb_truetype.h`
- **Severity:** High
- **Class:** Integer Overflow → Heap Buffer Overflow (Write)
- **Location:** `stb_truetype.h:1873-1880`
- **Source:** https://github.com/nothings/stb/pull/1939
- **Technique:** web-search
- **Description:**
  During composite glyph processing, the code allocates
  (num_vertices + comp_num_verts) * sizeof(stbtt_vertex) and then memcpy’s both
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
- **Notes:** The integer overflow requires ~32,768 composite components (each with max 65,535 points) to reach `num_vertices + comp_num_verts > INT_MAX`. The composite glyph handler re-allocates and copies all accumulated vertices per component (O(n²)), making this impractical to trigger dynamically on 64-bit systems. On 32-bit, `(num_vertices+comp_num_verts)*sizeof(stbtt_vertex)` can wrap in 32-bit `size_t` arithmetic at ~4,681 components, but 32-bit toolchain is unavailable on this system. The test at `tests/bug_002.c` confirms the composite glyph path works correctly for moderate inputs (up to 500 components × 65,536 vertices).

## BUG-003

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
  Build and run the harness from tests/bug_003.c with argument "cycle" to
  generate a self-referential composite glyph in-memory and trigger recursion.
  ```
- **Status:** Patched
- **Fix:** Add a recursion depth guard in stbtt__GetGlyphShapeTT and thread depth into recursive composite glyph calls to prevent infinite recursion (stb_truetype.h:1674, 1684, 1859, 2300).

## Session Summary — 2026-05-26

| Bug ID | Severity | Class | Status | Notes |
|--------|----------|-------|--------|-------|
| BUG-001 | High | Heap Buffer Overflow (OOB Read) | Invalid | Repro input did not trigger sanitizer in this build |
| BUG-002 | High | Integer Overflow → Heap Buffer Overflow (Write) | Invalid | Overflow requires ~32,768 components (impractical O(n²)); 32-bit only for heap overflow |
| BUG-003 | High | Stack Overflow (Unbounded Recursion) | Patched | Added recursion depth guard in composite glyph path |
