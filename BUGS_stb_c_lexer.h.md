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

## BUG-stb_c_lexer-006

- **Library:** `stb_c_lexer.h`
- **Severity:** High
- **Class:** OOB Read
- **Location:** `stb_c_lexer.h:308`
- **Source:** https://github.com/nothings/stb/issues/1118
- **Technique:** web-search
- **Description:**
  The `stb__clex_token` helper always sets `lexer->parse_point = end+1`. When a token's end
  coincides with `lexer->eof` (possible when a char literal is the last token in the input),
  `parse_point` advances one byte past the end of the input buffer. The next call to
  `stb_c_lexer_get_token` reads from `parse_point` which is at `eof+1`, causing an OOB read
  or a crash.
- **Reproduction sketch:**
  ```c
  char input[] = "'a'";
  char store[16];
  stb_lexer lex;
  stb_c_lexer_init(&lex, input, input + sizeof(input)-1, store, sizeof(store));
  stb_c_lexer_get_token(&lex);  // returns CLEX_charlit with end == eof
  stb_c_lexer_get_token(&lex);  // parse_point = eof+1, OOB read
  ```
- **Status:** Patched
- **Fix:** Added `(end == lexer->eof) ? end : end+1` guard to `stb__clex_token` at line 308, preventing `parse_point` from advancing past the buffer when a token ends exactly at EOF.

---

## BUG-stb_c_lexer-007

- **Library:** `stb_c_lexer.h`
- **Severity:** Medium
- **Class:** Incorrect Token Span
- **Location:** `stb_c_lexer.h:679`
- **Source:** https://github.com/nothings/stb/issues/1652
- **Technique:** web-search
- **Description:**
  When creating a char literal token, the end pointer is set to `p+1` where `p` already
  points at the closing `'`. This causes the token span to include one character after
  the closing quote. The subsequent `parse_point` skips the next character in the input,
  causing the lexer to miss tokens.
- **Reproduction sketch:**
  ```c
  char input[] = "'A';";
  char store[16];
  stb_lexer lex;
  stb_c_lexer_init(&lex, input, input + sizeof(input)-1, store, sizeof(store));
  stb_c_lexer_get_token(&lex);  // returns CLEX_charlit spanning "'A';" instead of "'A'"
  stb_c_lexer_get_token(&lex);  // ';' is skipped
  ```
- **Status:** Patched
- **Fix:** Changed `p+1` to `p` in the char-literal return at line 679, so the token end is the closing `'` rather than one byte past it. Prevents the lexer from consuming the character after the closing quote.

---

## Session Summary — 2026-06-01

| Bug ID | Severity | Class | Status | Notes |
|--------|----------|-------|--------|-------|
| BUG-stb_c_lexer-001 | High | OOB Read | Patched | Fixed at stb_c_lexer.h:475,491 — added eof guard to string parsing loop |
| BUG-stb_c_lexer-002 | High | OOB Read | Patched | Fixed at stb_c_lexer.h:574 — added eof guard to identifier continuation loop |
| BUG-stb_c_lexer-003 | Medium | OOB Read | Patched | Fixed at stb_c_lexer.h:671 — added eof guard before char literal parse |
| BUG-stb_c_lexer-004 | Low | OOB Read | Patched | Fixed at stb_c_lexer.h:532 — added eof guard to comment end detection |
| BUG-stb_c_lexer-005 | Low | OOB Read | Patched | Fixed at stb_c_lexer.h:521,529 — added eof guards to comment start detection |
| BUG-stb_c_lexer-006 | High | OOB Read | Patched | Fixed at stb_c_lexer.h:308 — parse_point no longer advances past eof |
| BUG-stb_c_lexer-007 | Medium | Incorrect Token Span | Patched | Fixed at stb_c_lexer.h:679 — char literal end no longer over-consumes |
