# BUGS_stb_easy_font.md

## BUG-stb_easy_font-001

- **Library:** stb_easy_font.h
- **Severity:** High
- **Class:** Out-of-bounds Read
- **Location:** stb_easy_font.h:213, 216-219, 239
- **Source:** static-analysis
- **Technique:** static-analysis
- **Description:**
  The functions `stb_easy_font_print`, `stb_easy_font_width`, and `stb_easy_font_height`
  lack validation for the input character range. They subtract 32 from the character
  value and use it as an index into `stb_easy_font_charinfo`, which has 96 entries.
  Input characters < 32 or > 126 will cause out-of-bounds reads. Specifically,
  character 127 is problematic because `stb_easy_font_print` accesses `index + 1`,
  leading to an OOB read at index 96. Also, since `char` is often signed,
  characters > 127 result in negative indices.
- **Reproduction sketch:**
  ```c
  #define STB_EASY_FONT_IMPLEMENTATION
  #include "stb_easy_font.h"
  #include <stdio.h>
  int main() {
      char text[2] = { (char)255, 0 };
      stb_easy_font_width(text);
      return 0;
  }
  ```
- **Status:** Patched
- **Fix:** Added character range checks (32-126) and used explicit `unsigned char` casts for character indexing.

## BUG-stb_easy_font-002

- **Library:** stb_easy_font.h
- **Severity:** Medium
- **Class:** Integer Overflow / Buffer Overflow
- **Location:** stb_easy_font.h:179
- **Source:** static-analysis
- **Technique:** static-analysis
- **Description:**
  In `stb_easy_font_draw_segs`, the check `offset+64 <= vbuf_size` can overflow if
  `offset` is large and `vbuf_size` is near `INT_MAX`. If it overflows, it may
  bypass the bounds check and perform out-of-bounds writes to the vertex buffer.
- **Reproduction sketch:**
  ```c
  unsigned char segs[1] = { 1 };
  stb_easy_font_draw_segs(0, 0, segs, 1, 0, c, vbuf, INT_MAX, INT_MAX - 32);
  ```
- **Status:** Patched
- **Fix:** Changed the overflow-prone check to `offset <= vbuf_size - 64` to ensure safe arithmetic.

## BUG-stb_easy_font-004

- **Library:** stb_easy_font.h
- **Severity:** Medium
- **Class:** NULL Pointer Dereference (Write)
- **Location:** stb_easy_font.h:182-186, 201
- **Source:** static-analysis
- **Technique:** static-analysis
- **Description:**
  In `stb_easy_font_print`, the `vertex_buffer` parameter is cast to `char *vbuf` (line 201) and
  subsequently written to in `stb_easy_font_draw_segs` (lines 182-186) without any NULL check.
  If `vertex_buffer` is NULL and `vbuf_size >= 64`, then `draw_segs` will write vertex data to
  `NULL + offset`, causing a NULL pointer write. This is a more severe instance than BUG-003
  because it involves a write (not just a read), and the caller might reasonably configure small
  positive `vbuf_size` values expecting graceful truncation. An attacker who controls both the
  text input and the buffer parameters could cause a write-what-where condition, though the
  practical exploitability is limited since NULL-page mappings are rare on modern systems.
- **Reproduction sketch:**
  ```c
  #define STB_EASY_FONT_IMPLEMENTATION
  #include "stb_easy_font.h"
  int main() {
      char text[] = "Hello";
      char *buf = NULL;
      // vbuf_size >= 64 triggers write to NULL
      stb_easy_font_print(0, 0, text, NULL, buf, 64);
      return 0;
  }
  ```
- **Status:** Patched
- **Fix:** Added `if (vbuf == NULL) return 0;` guard at the top of `stb_easy_font_print` (line 203) to return 0 quads immediately when called with a NULL vertex buffer, preventing the null-pointer write in `stb_easy_font_draw_segs`.

## BUG-stb_easy_font-003

- **Library:** stb_easy_font.h
- **Severity:** Low
- **Class:** NULL Pointer Dereference
- **Location:** stb_easy_font.h:208, 234, 252
- **Source:** static-analysis
- **Technique:** static-analysis
- **Description:**
  All three public functions (`stb_easy_font_print`, `stb_easy_font_width`, `stb_easy_font_height`)
  dereference the `text` parameter in the loop condition `while (*text)` without any NULL check.
  If any of these functions are called with `text == NULL` (e.g., from code that reads untrusted
  input that may be NULL), a NULL pointer dereference occurs, leading to a crash (denial of service).
  While many C libraries treat NULL as a precondition violation, for robustness against untrusted
  callers a NULL check prevents potential crash-based denial-of-service scenarios.
- **Reproduction sketch:**
  ```c
  #define STB_EASY_FONT_IMPLEMENTATION
  #include "stb_easy_font.h"
  int main() {
      stb_easy_font_print(0, 0, NULL, NULL, NULL, 0);  // crash
      return 0;
  }
  ```
- **Status:** Patched
- **Fix:** Added `if (text == NULL) return 0;` guard at the start of all three public functions (`stb_easy_font_print`, `stb_easy_font_width`, `stb_easy_font_height`) to return 0 gracefully when called with a NULL string pointer, preventing the null-pointer load in the `while (*text)` loop condition.

## Session Summary — 2026-06-17

| Bug ID | Severity | Class | Status | Notes |
|--------|----------|-------|--------|-------|
| BUG-stb_easy_font-001 | High | Out-of-bounds Read | Patched | Added range checks for input characters. |
| BUG-stb_easy_font-002 | Medium | Integer Overflow | Patched | Fixed overflow-prone buffer size check. |
| BUG-stb_easy_font-003 | Low | NULL Pointer Dereference | Patched | Added NULL guard for `text` in all three functions. |
| BUG-stb_easy_font-004 | Medium | NULL Pointer Dereference (Write) | Patched | Added NULL guard for `vertex_buffer` in `stb_easy_font_print`. |
