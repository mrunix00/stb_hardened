# stb_hardened

This project aims to harden [stb](https://github.com/nothings/stb) libraries using AI agents.

## How to Use

When using an agent to search for bugs in this project, the prompt should specify the following:
| Parameter | Required | Default | Description |
| --------------- | -------- | -------------- | --------------------------------------------------------------------------------------------------------------------- |
| `LIBRARY` | **Yes** | — | The STB header to target (e.g. `stb_image.h`, `stb_truetype.h`). |
| `TECHNIQUE` | No | Agent's choice | Discovery method: `web-search`, `fuzzing`, or `static-analysis`. |
| `AUTO_PROGRESS` | No | `false` | Whether the agent should automatically advance through all phases without waiting for user confirmation between them. |

## Example Prompts

```md
Harden stb_image.h using static analysis. Do not auto-progress.
```

```md
Harden stb_vorbis.c using fuzzing. Auto-progress through all phases.
```

```md
Harden stb_truetype.h. Let the agent pick the technique.
```

## Test and Fuzzer Infrastructure

The root `Makefile` provides the hardening workflow entry points. Builds default
to Clang-compatible ASan + UBSan instrumentation and write outputs under
`build/`.

```sh
make test
make test-bugs
make fuzz-build
make fuzz-smoke
```

Hardening regression tests belong in `tests/bug_NNN.c`. Each test should be
self-contained and should exit 0 only when the target bug is fixed. During
Phase 2, run the test against the source tree to validate that the sanitizer or
test assertion catches the issue. During Phase 3, re-run the same test with the
patched include directory first:

```sh
make test-bugs
make test-bugs STB_INCLUDE_DIR=patches BUILD_DIR=build/patched
```

Fuzzer builds include both a standalone replay binary and a libFuzzer binary:

```sh
make fuzz-build
build/tests/image_fuzzer_standalone path/to/crashcase
build/tests/stbi_read_fuzzer -runs=1000 tests/pngsuite/primary_check
```

Use `STB_INCLUDE_DIR=patches` when verifying patched headers saved under
`patches/<header-filename>`.
