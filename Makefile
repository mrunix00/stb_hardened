TEST_MAKEFILE := tests/Makefile
BUILD_DIR ?= build

.PHONY: help test test-stock test-bugs fuzz-build fuzz-smoke clean

help:
	@printf '%s\n' \
		'Hardening infrastructure targets:' \
		'  make test                 Build stock compile tests and run tests/bug_*.c' \
		'  make test-stock           Build the upstream-style stb compile targets' \
		'  make test-bugs            Build and run hardening regression tests in tests/bug_*.c' \
		'  make fuzz-build           Build standalone and libFuzzer image fuzzers' \
		'  make fuzz-smoke           Build fuzzers and run a short libFuzzer smoke pass' \
		'  make bench                Build and run all benchmark suites' \
		'  make clean                Remove build outputs' \
		'' \
		'Useful variables:' \
		'  STB_INCLUDE_DIR=patches   Prefer patched headers in patches/' \
		'  BUILD_DIR=build/source    Select output directory' \
		'  CC=clang CXX=clang++      Select compilers' \
		'  SANITIZERS=address,undefined'

test:
	$(MAKE) -f $(TEST_MAKEFILE) all bug-tests BUILD_DIR=$(BUILD_DIR)/tests

test-stock:
	$(MAKE) -f $(TEST_MAKEFILE) all BUILD_DIR=$(BUILD_DIR)/tests

test-bugs:
	$(MAKE) -f $(TEST_MAKEFILE) bug-tests BUILD_DIR=$(BUILD_DIR)/tests

fuzz-build:
	$(MAKE) -f $(TEST_MAKEFILE) fuzzers BUILD_DIR=$(BUILD_DIR)/tests

fuzz-smoke:
	$(MAKE) -f $(TEST_MAKEFILE) fuzz-smoke BUILD_DIR=$(BUILD_DIR)/tests

clean:
	$(MAKE) -f $(TEST_MAKEFILE) clean BUILD_DIR=$(BUILD_DIR)/tests
	rm -rf $(BUILD_DIR)

bench: bench-stb-image bench-stb-truetype bench-stb-image-resize2 bench-stb-image-write bench-stb-hexwave bench-stb-vorbis bench-stb-c-lexer

bench-stb-image:
	$(MAKE) -f $(TEST_MAKEFILE) $(BUILD_DIR)/tests/bench_stb_image BUILD_DIR=$(BUILD_DIR)/tests
	$(BUILD_DIR)/tests/bench_stb_image

bench-stb-truetype:
	$(MAKE) -f $(TEST_MAKEFILE) $(BUILD_DIR)/tests/bench_stb_truetype BUILD_DIR=$(BUILD_DIR)/tests
	$(BUILD_DIR)/tests/bench_stb_truetype

bench-stb-image-resize2:
	$(MAKE) -f $(TEST_MAKEFILE) $(BUILD_DIR)/tests/bench_stb_image_resize2 BUILD_DIR=$(BUILD_DIR)/tests
	$(BUILD_DIR)/tests/bench_stb_image_resize2

bench-stb-image-write:
	$(MAKE) -f $(TEST_MAKEFILE) $(BUILD_DIR)/tests/bench_stb_image_write BUILD_DIR=$(BUILD_DIR)/tests
	$(BUILD_DIR)/tests/bench_stb_image_write

bench-stb-hexwave:
	$(MAKE) -f $(TEST_MAKEFILE) $(BUILD_DIR)/tests/bench_stb_hexwave BUILD_DIR=$(BUILD_DIR)/tests
	$(BUILD_DIR)/tests/bench_stb_hexwave

bench-stb-vorbis:
	$(MAKE) -f $(TEST_MAKEFILE) $(BUILD_DIR)/tests/bench_stb_vorbis BUILD_DIR=$(BUILD_DIR)/tests
	$(BUILD_DIR)/tests/bench_stb_vorbis

bench-stb-c-lexer:
	$(MAKE) -f $(TEST_MAKEFILE) $(BUILD_DIR)/tests/bench_stb_c_lexer BUILD_DIR=$(BUILD_DIR)/tests
	$(BUILD_DIR)/tests/bench_stb_c_lexer
