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

## BUG-stb_c_lexer-008

- **Library:** `stb_c_lexer.h`
- **Severity:** High
- **Class:** OOB Read
- **Location:** `stb_c_lexer.h:477-481`
- **Source:** Static analysis (manual code audit)
- **Technique:** static-analysis
- **Description:**
  In `stb__clex_parse_string`, when the character before `lexer->eof` is a backslash (`\`), the code unconditionally calls `stb__clex_parse_char(p, &q)`. This function reads `p[1]` unconditionally when `*p == '\\'` (to determine the escape type), but `p` is at `lexer->eof-1`, so `p[1]` reads one byte past the end of the input buffer. Depending on what byte is at `eof`:
  - If it matches a recognized escape char (e.g. `'n'`, `'t'`, `'0'`, etc.), the function returns successfully with `*q = p+2 = eof+1`. The caller then sets `p = q = eof+1`, causing subsequent iterations to read past the buffer.
  - If it matches `'x'`, `'X'`, or `'u'` (return -1 cases), `*q = eof+1` persists and the error token carries `end = eof+1`, setting `parse_point = eof+2` for the next call.
  - Example input that triggers this: `"\"\\` (opening quote + backslash as the last bytes before eof).
- **Reproduction sketch:**
  ```c
  char input[] = "\"\\";  // unterminated string with trailing backslash
  char store[256];
  stb_lexer lex;
  stb_c_lexer_init(&lex, input, input + sizeof(input)-1, store, sizeof(store));
  // stb_c_lexer_get_token(&lex) reads byte at eof in stb__clex_parse_char's switch
  ```
- **Status:** Patched
- **Fix:** Added `p+1 == lexer->eof` guard before calling `stb__clex_parse_char` in the string-escape branch at `stb_c_lexer.h:479-483`. When the backslash is at the last byte of input, it is treated as a literal character (consistent with how unknown escape sequences are handled), preventing the OOB read in `stb__clex_parse_char`'s `switch(p[1])`.

---

## BUG-stb_c_lexer-009

- **Library:** `stb_c_lexer.h`
- **Severity:** Medium
- **Class:** OOB Read
- **Location:** `stb_c_lexer.h:674`
- **Source:** Static analysis (manual code audit)
- **Technique:** static-analysis
- **Description:**
  In the char-literal parsing branch, `stb__clex_parse_char(p+1, &p)` is called after confirming `p+1 != lexer->eof` (line 672). However, if `p == lexer->eof-2`, then the argument to `stb__clex_parse_char` is `lexer->eof-1`. If `*(eof-1) == '\\'`, the function reads `p[1] = *(eof)` in its `switch(p[1])` statement — one byte past the buffer. This is the same root cause as BUG-008 but triggered through the char-literal path rather than the string path.
- **Reproduction sketch:**
  ```c
  char input[] = "'\\";  // char literal with trailing backslash
  char store[16];
  stb_lexer lex;
  stb_c_lexer_init(&lex, input, input + sizeof(input)-1, store, sizeof(store));
  // stb_c_lexer_get_token(&lex) reads byte at eof in stb__clex_parse_char
  ```
- **Status:** Patched
- **Fix:** Added `p+2 == lexer->eof` guard alongside the existing `p+1 == lexer->eof` check at `stb_c_lexer.h:674`. Since we cannot know ahead of time whether the character after `'` is a backslash (which would cause `stb__clex_parse_char` to read `p[1]`), the tighter bound ensures at least 2 bytes are available after the opening quote for the escape sequence.

---

## BUG-stb_c_lexer-010

- **Library:** `stb_c_lexer.h`
- **Severity:** Medium
- **Class:** OOB Read
- **Location:** `stb_c_lexer.h:696, 711, 749, 787`
- **Source:** Static analysis (manual code audit)
- **Technique:** static-analysis
- **Description:**
  When `STB_C_LEX_USE_STDLIB` is `Y` (the default), the numeric parsing paths use `strtol()` and `strtod()` to parse number tokens. These C standard library functions operate on null-terminated strings and do not accept a length bound. If the input buffer is not null-terminated — which is allowed by the API (the user provides both `input_stream` and `input_stream_end`) — `strtol`/`strtod` will read past `lexer->eof`, scanning past the intended input until they encounter a non-numeric byte or a null terminator in adjacent memory.
  
  The non-stdlib parsing paths (`#ifndef STB__CLEX_use_stdlib`) do not have this issue because they use explicit `q != lexer->eof` checks in their parsing loops.
  
  Affected call sites:
  - Line 696: `strtod` for hex float parsing
  - Line 711: `strtol` for hex integer parsing
  - Line 749: `strtod` for decimal float parsing
  - Line 787: `strtol` for decimal integer parsing
  
  After the unbounded read, the returned pointer `q` may point past `lexer->eof`. This value is passed to `stb__clex_parse_suffixes` (which with default settings simply passes it through as the token's end pointer), causing `lexer->parse_point` to be set past the buffer for the next token call.

- **Reproduction sketch:**
  ```c
  char buf[] = "12345";  // buffer NOT null-terminated; adjacent memory may extend
  char store[16];
  stb_lexer lex;
  // sizeof(buf) includes the implicit null terminator — but if a user constructed
  // the buffer without one and set input_stream_end explicitly, strtol over-reads.
  stb_c_lexer_init(&lex, buf, buf + sizeof(buf) - 1, store, sizeof(store));
  // stb_c_lexer_get_token(&lex) may read past buf+5 via strtol
  ```
- **Status:** Patched
- **Fix:** Changed the default of `STB_C_LEX_USE_STDLIB` from `Y` to `N` at `stb_c_lexer.h:93`. The library's own bounded parser (which checks `q != lexer->eof` in its loops) is now used by default, eliminating the unbounded read at all four call sites. Users who require exact stdlib parsing can opt in by defining `#define STB_C_LEX_USE_STDLIB Y` before including the header, provided their input is null-terminated.

---

## BUG-stb_c_lexer-011

- **Library:** `stb_c_lexer.h`
- **Severity:** Medium
- **Class:** OOB Read (Heap Buffer Overflow)
- **Location:** `stb_c_lexer.h:339`
- **Source:** libFuzzer crash — input: `1` (single byte), heap-buffer-overflow in `stb__clex_parse_suffixes`
- **Technique:** fuzzing
- **Description:**
When `STB_C_LEX_PARSE_SUFFIXES` is `Y`, the suffix-parsing function `stb__clex_parse_suffixes` enters a `while ((*cur >= 'a' && *cur <= 'z') || (*cur >= 'A' && *cur <= 'Z'))` loop that reads `*cur` without checking whether `cur` has reached `lexer->eof`. If the number being parsed is the last token in the input (so `cur == eof` after parsing the digits), the loop dereferences `lexer->eof` — one byte past the end of the input buffer. With ASan this is caught as a heap-buffer-overflow read of size 1.
  
The trigger is any numeric token at the end of the input when suffix parsing (a non-default feature) is enabled.
- **Reproduction sketch:**
```c
char buf[] = "1"; // single digit, no trailing bytes
stb_lexer lex;
stb_c_lexer_init(&lex, buf, buf + 1, store, store_len);
// stb_c_lexer_get_token(&lex) parses "1", calls stb__clex_parse_suffixes with cur == eof
// stb__clex_parse_suffixes reads *cur == *eof -> OOB
```
- **Status:** Patched
- **Fix:** Added `cur != lexer->eof` to the suffix-letter loop guard at `stb_c_lexer.h:339` so suffix parsing stops before dereferencing one-past-end input.

---

## BUG-stb_c_lexer-012

- **Library:** `stb_c_lexer.h`
- **Severity:** High
- **Class:** OOB Read (Heap Buffer Overflow)
- **Location:** `stb_c_lexer.h:393-408` (specifically line 397)
- **Source:** libFuzzer crash — input: `8.` (digit followed by decimal point at end), heap-buffer-overflow in `stb__clex_parse_float`
- **Technique:** fuzzing
- **Description:**
In `stb__clex_parse_float`, after matching a decimal point (`'.'`), the code unconditionally enters `for (pow=1; ; pow*=base)` and reads `*p` on the first iteration without checking whether `p` points past `lexer->eof`. When the decimal point is the last character in the input (e.g. `8.` at end of buffer), `p` becomes `eof` after the `++p` past the `'.'`, and the loop dereferences `*eof` — one byte past the end.
  
This is reachable in the default configuration with inputs like a number immediately followed by a trailing decimal point with nothing after it.
- **Reproduction sketch:**
```c
char buf[] = "8.";  // digit + dot, no trailing bytes
stb_lexer lex;
stb_c_lexer_init(&lex, buf, buf + 2, store, store_len);
// stb_c_lexer_get_token(&lex) parses this as a decimal float
// stb__clex_parse_float reads *eof in the fraction-digit for loop
```
- **Status:** Patched
- **Fix:** Added eof guard to `stb__clex_parse_float` at `stb_c_lexer.h:396` — the fractional-digit for-loop and exponent-read now check `p != eof` before dereferencing. Also added eof parameter to the function signature and guarded the exponent-digit while loop at `stb_c_lexer.h:428`.

## Session Summary — 2026-06-17

| Bug ID | Severity | Class | Status | Notes |
|--------|----------|-------|--------|-------|
| BUG-stb_c_lexer-001 | High | OOB Read | Patched | Fixed at stb_c_lexer.h:475,491 — added eof guard to string parsing loop |
| BUG-stb_c_lexer-002 | High | OOB Read | Patched | Fixed at stb_c_lexer.h:574 — added eof guard to identifier continuation loop |
| BUG-stb_c_lexer-003 | Medium | OOB Read | Patched | Fixed at stb_c_lexer.h:671 — added eof guard before char literal parse |
| BUG-stb_c_lexer-004 | Low | OOB Read | Patched | Fixed at stb_c_lexer.h:532 — added eof guard to comment end detection |
| BUG-stb_c_lexer-005 | Low | OOB Read | Patched | Fixed at stb_c_lexer.h:521,529 — added eof guards to comment start detection |
| BUG-stb_c_lexer-006 | High | OOB Read | Patched | Fixed at stb_c_lexer.h:308 — parse_point no longer advances past eof |
| BUG-stb_c_lexer-007 | Medium | Incorrect Token Span | Patched | Fixed at stb_c_lexer.h:679 — char literal end no longer over-consumes |
| BUG-stb_c_lexer-008 | High | OOB Read | Patched | String escape reads past eof when backslash is last byte — fixed at stb_c_lexer.h:479-483 |
| BUG-stb_c_lexer-009 | Medium | OOB Read | Patched | Char-literal escape reads past eof when escape at eof-2 — fixed at stb_c_lexer.h:674 |
| BUG-stb_c_lexer-010 | Medium | OOB Read | Patched | strtol/strtod unbounded read — fixed by changing default to bounded parser at stb_c_lexer.h:93 |

## Session Summary — 2026-07-06

| Bug ID | Severity | Class | Status | Notes |
|--------|----------|-------|--------|-------|
| BUG-stb_c_lexer-011 | Medium | OOB Read (Heap Buffer Overflow) | Patched | Fixed at `stb_c_lexer.h:339`; added EOF guard to suffix parsing loop. |
