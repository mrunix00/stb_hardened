# BUGS — stb_c_lexer.h

## BUG-stb_c_lexer-001

- **Library:** `stb_c_lexer.h`
- **Severity:** High
- **Class:** OOB Read
- **Location:** `stb_c_lexer.h:475-491`
- **Source:** Static analysis (manual code audit)
- **Technique:** web-search
- **Description:**
  The string parsing function `stb__clex_parse_string` (called when a `"` or `'` token is encountered) reads input in a `while (*p != delim)` loop that checks neither `p != lexer->eof` nor any other end-of-input guard. If the input does not contain a closing delimiter, the loop scans past the end of the input buffer, reading arbitrary memory until it happens to find a byte matching `delim` or faults.
- **Reproduction sketch:**
  ```c
  char input[] = "\"unterminated";
  char store[256];
  stb_lexer lex;
  stb_c_lexer_init(&lex, input, input + sizeof(input)-1, store, sizeof(store));
  // stb_c_lexer_get_token(&lex) reads past input[] scanning for the closing '"'
  ```
- **Status:** Patched
- **Fix:** Added `p != lexer->eof` guard to the `while` loop in `stb__clex_parse_string` and an EOF check after the loop that returns `CLEX_parse_error`. Changed lines 475 and 491-492 in `stb_c_lexer.h`.

---

## BUG-stb_c_lexer-002

- **Library:** `stb_c_lexer.h`
- **Severity:** High
- **Class:** OOB Read
- **Location:** `stb_c_lexer.h:571-577`
- **Source:** Static analysis (manual code audit)
- **Technique:** web-search
- **Description:**
  The identifier continuation loop in `stb_c_lexer_get_token` reads `p[n]` without checking whether `p+n` exceeds `lexer->eof`. If an identifier extends to the end of the input, the loop reads past the buffer scanning for non-identifier characters. The only bounds check inside the loop is against `string_storage_len` (line 567), not against input length.
- **Reproduction sketch:**
  ```c
  char input[] = "a12345";  // no null terminator or eof sentinel
  char store[16];
  stb_lexer lex;
  stb_c_lexer_init(&lex, input, input + sizeof(input)-1, store, sizeof(store));
  // stb_c_lexer_get_token(&lex) reads past input[] looking for non-id chars
  ```
- **Status:** Patched
- **Fix:** Added `p+n != lexer->eof` guard to the `while` condition in the identifier continuation loop at line 574 in `stb_c_lexer.h`.

---

## BUG-stb_c_lexer-003

- **Library:** `stb_c_lexer.h`
- **Severity:** Medium
- **Class:** OOB Read
- **Location:** `stb_c_lexer.h:670`
- **Source:** Static analysis (manual code audit)
- **Technique:** web-search
- **Description:**
  In the character-literal parsing branch, `stb__clex_parse_char(p+1, &p)` is called without verifying that `p+1 < lexer->eof`. If the input ends with a lone `'` character and no following character, `p+1` equals `lexer->eof`, and `stb__clex_parse_char` dereferences the eof pointer when checking `*p == '\\'`. This reads one byte past the end of the input buffer.
- **Reproduction sketch:**
  ```c
  char input[] = "'";
  char store[16];
  stb_lexer lex;
  stb_c_lexer_init(&lex, input, input + sizeof(input)-1, store, sizeof(store));
  // stb_c_lexer_get_token(&lex) reads *p where p == eof
  ```
- **Status:** Patched
- **Fix:** Added `p+1 == lexer->eof` guard before calling `stb__clex_parse_char` in the char-literal branch at line 671 in `stb_c_lexer.h`.

---

## BUG-stb_c_lexer-004

- **Library:** `stb_c_lexer.h`
- **Severity:** Low
- **Class:** OOB Read
- **Location:** `stb_c_lexer.h:530`
- **Source:** Static analysis (manual code audit)
- **Technique:** web-search
- **Description:**
  The block-comment end-detection condition `(p[0] != '*' || p[1] != '/')` reads `p[1]` when `p == lexer->eof-1`. The loop guard `p != lexer->eof` passes, but if the last byte of input is `*`, the byte at `lexer->eof` (one past the buffer) is read. This is a one-byte OOB read.
- **Reproduction sketch:**
  ```c
  char input[] = "/* xxx *";  // unterminated comment ending with '*'
  char store[16];
  stb_lexer lex;
  stb_c_lexer_init(&lex, input, input + sizeof(input)-1, store, sizeof(store));
  // stb_c_lexer_get_token(&lex) reads byte at eof
  ```
- **Status:** Patched
- **Fix:** Added `p+1 == lexer->eof` guard to block comment end detection at line 530 in `stb_c_lexer.h`.

---

## BUG-stb_c_lexer-005

- **Library:** `stb_c_lexer.h`
- **Severity:** Low
- **Class:** OOB Read
- **Location:** `stb_c_lexer.h:519`
- **Source:** Static analysis (manual code audit)
- **Technique:** web-search
- **Description:**
  The C++ comment detection `p[0] == '/' && p[1] == '/'` reads `p[1]` when `p == lexer->eof-1`. The check `p != lexer->eof` passes for the last byte, but `p[1]` reads at `lexer->eof`, which is one byte past the buffer.
- **Reproduction sketch:**
  ```c
  char input[] = "/";  // lone '/'
  char store[16];
  stb_lexer lex;
  stb_c_lexer_init(&lex, input, input + sizeof(input)-1, store, sizeof(store));
  // stb_c_lexer_get_token(&lex) reads byte at eof
  ```
- **Status:** Patched
- **Fix:** Added `p+1 != lexer->eof` guard to `//` and `/*` comment detection checks at lines 521 and 529 in `stb_c_lexer.h`.

---

## Session Summary — 2026-05-31

| Bug ID | Severity | Class | Status | Notes |
|--------|----------|-------|--------|-------|
| BUG-stb_c_lexer-001 | High | OOB Read | Patched | Fixed at stb_c_lexer.h:475,491 — added eof guard to string parsing loop |
| BUG-stb_c_lexer-002 | High | OOB Read | Patched | Fixed at stb_c_lexer.h:574 — added eof guard to identifier continuation loop |
| BUG-stb_c_lexer-003 | Medium | OOB Read | Patched | Fixed at stb_c_lexer.h:671 — added eof guard before char literal parse |
| BUG-stb_c_lexer-004 | Low | OOB Read | Patched | Fixed at stb_c_lexer.h:532 — added eof guard to comment end detection |
| BUG-stb_c_lexer-005 | Low | OOB Read | Patched | Fixed at stb_c_lexer.h:521,529 — added eof guards to comment start detection |
