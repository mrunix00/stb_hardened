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

## Session Summary — 2026-05-30

| Bug ID | Severity | Class | Status | Notes |
|--------|----------|-------|--------|-------|
| BUG-stb_easy_font-001 | High | Out-of-bounds Read | Patched | Added range checks for input characters. |
| BUG-stb_easy_font-002 | Medium | Integer Overflow | Patched | Fixed overflow-prone buffer size check. |
