# STB Library Hardening Agent

This document defines the workflow for an agent tasked with discovering, validating, and patching security vulnerabilities in [nothings/stb](https://github.com/nothings/stb) single-header libraries.

---

## Invocation

When starting a session, the following parameters must be provided in the initial prompt:

| Parameter | Required | Default | Description |
|---|---|---|---|
| `LIBRARY` | **Yes** | — | The STB header to target (e.g. `stb_image.h`, `stb_truetype.h`). |
| `TECHNIQUE` | No | Agent's choice | Discovery method: `web-search`, `fuzzing`, or `static-analysis`. |
| `AUTO_PROGRESS` | No | `false` | Whether the agent should skip the Phase 1 → Phase 2 confirmation gate and run through all phases unattended. |

### Example prompts

```
Harden stb_image.h using static analysis. Do not auto-progress.
```

```
Harden stb_vorbis.c using fuzzing. Auto-progress through all phases.
```

```
Harden stb_truetype.h — let the agent pick the technique.
```

---

## Output Artifacts

| File | Written by | Purpose |
|---|---|---|
| `BUGS_<library>.md` | Phase 1 | Catalogues every discovered issue for a given library. |
| `tests/bug_<library>_<NNN>.c` | Phase 2 | Self-contained test that reproduces and later validates the fix for `BUG-<library>-<NNN>`. |

All output files must be created relative to the working directory of the session.

---

## Session Start — State Check

Before doing anything else, the agent must check whether `BUGS_<library>.md` already exists for the target library and contains entries with status `Unvalidated` that were discovered using the specified (or chosen) technique.

```
if BUGS_<library>.md exists AND contains Unvalidated entries for LIBRARY discovered via TECHNIQUE:
    → Skip Phase 1 entirely. Go to "Resuming from existing bugs" below.
else:
    → Check whether BUGS.md (legacy monolithic file) exists with relevant entries; if so, migrate them.
    → Run Phase 1 normally.
```

### Resuming from existing bugs

The agent announces how many `Unvalidated` entries it found and which bug to work on next:

**When `AUTO_PROGRESS: true`:**
> The agent silently picks the highest-severity unvalidated bug and proceeds directly to Phase 2 for that bug, emitting only a one-line status message:
> ```
> Resuming session. Picked BUG-<library>-<NNN> (<severity> — <one-line description>). Starting validation.
> ```

**When `AUTO_PROGRESS: false`:**
> The agent lists all `Unvalidated` entries and asks the user to choose:
> ```
> Found N unvalidated bugs from a previous discovery run:
>   [1] BUG-<library>-001 — High   — Integer overflow in image width calculation
>   [2] BUG-<library>-003 — Medium — Unchecked malloc return in font loader
>   ...
>   [D] Run discovery instead to find new bugs
>
> Which bug should we validate next? (enter a number or D)
> ```
> If the user picks `D`, the agent runs Phase 1 normally and appends any new findings to `BUGS_<library>.md`.

---

## Phase 1 — Discovery

**Goal:** Identify as many potential security issues as possible in the target library and record all of them in `BUGS_<library>.md`. Discovery is exhaustive — the agent documents everything it finds before the session moves on.

The agent selects (or is told) one of three techniques:

### Technique A — Web Search

1. Search GitHub Issues, GitHub Security Advisories, and the NVD/CVE database for known vulnerabilities affecting the target header.
2. Cross-reference any related projects or forks that may have independently reported or fixed issues.
3. Record every finding, even unconfirmed ones, with a source URL.

### Technique B — Fuzzing

1. Inspect the target header's public API to identify entry points that accept external data (file paths, raw buffers, sizes, format strings, etc.).
2. Write or adapt a fuzzing harness (preferably using [libFuzzer](https://llvm.org/docs/LibFuzzer.html) or [AFL++](https://aflplus.plus/)) that feeds mutated inputs into those entry points.
3. Run the fuzzer for a meaningful number of iterations; collect any crashes, hangs, or sanitizer reports (ASan / UBSan / MSan).
4. Minimise and de-duplicate corpus entries before moving on.

### Technique C — Static Analysis

1. Fetch or locate the source of the target header.
2. Read through the code and flag suspicious patterns, including but not limited to:
   - Unbounded `memcpy` / `strcpy` / `sprintf` calls.
   - Integer overflow / underflow before allocation or indexing.
   - Off-by-one errors in loop bounds or buffer indices.
   - Unchecked return values from malloc / realloc.
   - Use-after-free or double-free patterns.
   - Signed/unsigned mismatch used in size computations.
   - Reachable `assert`s that should be hard errors.
3. Annotate each finding with file name, line number(s), and a brief explanation.

### `BUGS_<library>.md` entry format

Each entry must follow this template exactly. Every distinct root cause gets its own entry — the agent must not skip or merge entries to keep the list short.

```markdown
## BUG-<library>-<NNN>

- **Library:** `<header filename>`
- **Severity:** Critical | High | Medium | Low | Informational
- **Class:** (e.g. Heap Buffer Overflow, Integer Overflow, OOB Read, Use-After-Free, …)
- **Location:** `<filename>:<line-range>` (omit if discovered via web search only)
- **Source:** <CVE-ID / GitHub Issue URL / fuzzer crash ID / static-analysis note>
- **Technique:** web-search | fuzzing | static-analysis
- **Description:**
  A concise explanation of the vulnerability: what the flawed code does, how it can
  be triggered, and what an attacker could achieve.
- **Reproduction sketch:**
  A minimal pseudo-code or command-line snippet that would trigger the issue.
- **Status:** Unvalidated
```

### End of Phase 1

After all findings are written to `BUGS_<library>.md`, the agent reports the count and pauses:

**`AUTO_PROGRESS: false`:**
```
Phase 1 complete. Documented N potential vulnerabilities in BUGS_<library>.md.
Awaiting your go-ahead to begin Phase 2 (Validation).
You can also review BUGS_<library>.md and remove any entries you do not want validated before confirming.
```

**`AUTO_PROGRESS: true`:**
```
Phase 1 complete. Documented N potential vulnerabilities in BUGS_<library>.md.
Picking highest-severity entry to validate first.
```

---

## Phase 2 — Validation

**Goal:** Confirm that a **single chosen bug** is a real, reproducible issue and not a false positive, by implementing and running an automated test against the unpatched library.

1. Implement a self-contained test in `tests/bug_<library>_<NNN>.c` that:
   - Sets up the minimal conditions required to trigger the bug.
   - Is expected to crash, produce a sanitizer report, or return an incorrect result when the bug is present.
   - Passes cleanly (exit 0, no sanitizer output) once the bug is fixed.
2. Compile and run the test **against the unpatched library** with `-fsanitize=address,undefined` (add `-fsanitize=memory` where the toolchain supports it):
   - Test **fails as expected** → update the entry's status in `BUGS_<library>.md` to `Validated`. Proceed to Phase 3 immediately.
   - Test **passes** (no fault observed) → update status to `Invalid` and record a brief explanation of why the bug could not be reproduced (see [Bug statuses](#bug-statuses)). The agent then returns to the session start state to pick the next unvalidated bug.

> Validation is strictly per-bug. Do not attempt to validate multiple bugs in the same Phase 2 run.

---

## Phase 3 — Patching

**Goal:** Fix the single `Validated` bug from Phase 2 with the smallest correct change possible, then confirm the fix passes the test written in Phase 2.

1. Apply the most minimal patch possible directly to the target library source file — change only the exact lines that introduce or permit the vulnerability. A one-line fix is preferred over a multi-line fix; a multi-line fix is preferred over a structural refactor. Do not rename variables, reformat code, or adjust anything outside the direct fix, even if the surrounding code is of poor quality.
2. Re-run `tests/bug_<library>_<NNN>.c` against the patched library:
   - Test **passes** → update the entry status in `BUGS_<library>.md` to `Patched`. Add a `**Fix:**` field to the entry with a one-line rationale and the exact lines changed.
   - Test **still fails** → the patch is incorrect. Revise and retry. Do not mark the bug as `Patched` until the test passes cleanly.
3. Run any previously-passing tests in `tests/` to check for regressions. If a regression is found, fix it before proceeding.

> If fixing this bug reveals or depends on another bug in `BUGS_<library>.md`, note the relationship explicitly on both entries.

### After patching

The agent returns to the **Session Start — State Check** step, now treating the just-patched bug as resolved and picking the next `Unvalidated` entry (or prompting the user to pick one, depending on `AUTO_PROGRESS`).

This loop continues — one bug validated and patched per iteration — until the user ends the session or no `Unvalidated` entries remain.

When no `Unvalidated` entries remain, the agent prints a final session summary (see [Session Summary](#session-summary)).

---

## Bug Statuses

The `Status` field on each `BUGS_<library>.md` entry follows this state machine:

```
Unvalidated
    │
    ├─── test fails as expected ──────► Validated
    │                                       │
    │                                       └─── patch passes test ──► Patched
    │
    └─── test passes (not reproducible) ──► Invalid
```

| Status | Meaning |
|---|---|
| `Unvalidated` | Discovered but not yet tested. |
| `Validated` | Automated test confirmed the bug is reproducible. |
| `Invalid` | Automated test could not reproduce the bug; considered a false positive. The test file is kept in `tests/` as documentation. |
| `Patched` | Fix applied and verified by the test. |

---

## Commit Message Format

When committing a fix, the message must follow the pattern:

```
<library>: <fix>
```

Where `<library>` is the short library name (e.g. `stb_truetype`, `stb_image`, `stb_image_resize2`) and `<fix>` is a concise description of the change.

Example:
```
stb_truetype: add recursion depth guard in composite glyph path
```

---

## Session Summary

Appended to `BUGS_<library>.md` at the end of each session (or when no `Unvalidated` entries remain):

```markdown
## Session Summary — <date>

| Bug ID | Severity | Class | Status | Notes |
|--------|----------|-------|--------|-------|
| BUG-<library>-001 | High | Integer Overflow | Patched | Fixed at stb_image.h:1234 |
| BUG-<library>-002 | Medium | OOB Read | Invalid | Could not reproduce; bounds check already present |
| BUG-<library>-003 | High | Heap Buffer Overflow | Unvalidated | Not reached this session |
```

---

## General Constraints

- **One bug at a time.** Validate and patch exactly one bug per Phase 2 / Phase 3 cycle. Do not batch fixes.
- **No false fixes.** Do not patch a bug that has not reached `Validated` status.
- **Minimal diffs.** Each patch must touch only the exact lines that introduce or permit the vulnerability. Prefer a one-line fix over a multi-line fix, and a multi-line fix over any structural change. No reformatting, no renames, no opportunistic cleanups — even if the surrounding code is poor.
- **Preserve API compatibility.** Public function signatures must not change unless the signature itself is the vulnerability (e.g. a missing `size` parameter).
- **Document everything.** Every decision — why a bug was marked `Invalid`, why a particular fix strategy was chosen — must be recorded in `BUGS_<library>.md`.
- **Sanitizer coverage is mandatory.** All tests must be compiled and run with ASan + UBSan enabled.
- **Do not silence warnings.** Fixes must not introduce `#pragma` suppressions or cast-away-const tricks to hide compiler diagnostics.
- **Patch source files directly.** Do not create `patches/` copies; the target library file itself must reflect all fixes applied so far in the session.
