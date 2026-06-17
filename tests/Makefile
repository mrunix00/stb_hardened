ifeq ($(origin CC),default)
CC := clang
endif
ifeq ($(origin CXX),default)
CXX := clang++
endif
export CCACHE_DISABLE := 1
TEST_DIR := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))
ROOT ?= $(TEST_DIR)/..
BUG_BIN_DIR ?= $(TEST_DIR)/bin
BUILD_DIR ?= $(BUG_BIN_DIR)
STB_INCLUDE_DIR ?= $(ROOT)
SANITIZERS ?= address,undefined
COMMON_WARNINGS ?= -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
COMMON_SANITIZE_FLAGS ?= -g -O1 -fno-omit-frame-pointer -fsanitize=$(SANITIZERS)
INCLUDES := -I$(STB_INCLUDE_DIR) -I$(ROOT)
CFLAGS ?=
CXXFLAGS ?=
TEST_CFLAGS := $(COMMON_WARNINGS) -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -DSTB_DIVIDE_TEST $(COMMON_SANITIZE_FLAGS) $(CFLAGS)
TEST_CXXFLAGS := $(COMMON_WARNINGS) -Wno-write-strings -DSTB_DIVIDE_TEST $(COMMON_SANITIZE_FLAGS) $(CXXFLAGS)
TEST_LDFLAGS := $(COMMON_SANITIZE_FLAGS) $(LDFLAGS)
FUZZ_SANITIZERS ?= address,undefined,fuzzer
FUZZ_CFLAGS := $(COMMON_WARNINGS) -g -O1 -fno-omit-frame-pointer -fsanitize=$(FUZZ_SANITIZERS) $(CFLAGS)
FUZZ_LDFLAGS := -fsanitize=$(FUZZ_SANITIZERS) $(LDFLAGS)
RUN_ENV ?= ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1:abort_on_error=1
FUZZ_RUN_ENV ?= ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1:abort_on_error=1

STOCK_C_SRCS := $(ROOT)/stb_vorbis.c $(TEST_DIR)/test_c_compilation.c $(TEST_DIR)/test_c_lexer.c $(TEST_DIR)/test_dxt.c $(TEST_DIR)/test_easyfont.c $(TEST_DIR)/test_image.c $(TEST_DIR)/test_image_write.c $(TEST_DIR)/test_perlin.c $(TEST_DIR)/test_sprintf.c $(TEST_DIR)/test_truetype.c $(TEST_DIR)/test_voxel.c
BUG_TEST_SRCS := $(sort $(wildcard $(TEST_DIR)/bug_*.c))
BUG_TEST_BINS := $(patsubst $(TEST_DIR)/%.c,$(BUILD_DIR)/%,$(BUG_TEST_SRCS))

.PHONY: all bug-tests fuzzers fuzz-smoke clean

all: $(BUILD_DIR)/stb_compile_c $(BUILD_DIR)/stb_compile_cpp $(BUILD_DIR)/image_write_test $(BUILD_DIR)/image_fuzzer_standalone

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/stb_compile_c: $(STOCK_C_SRCS) | $(BUILD_DIR)
	$(CC) $(INCLUDES) $(TEST_CFLAGS) $^ -lm $(TEST_LDFLAGS) -o $@

$(BUILD_DIR)/stb_compile_cpp: $(TEST_DIR)/test_cpp_compilation.cpp | $(BUILD_DIR)
	$(CXX) $(INCLUDES) $(TEST_CXXFLAGS) -std=c++11 -c $< -o $@

$(BUILD_DIR)/image_write_test: $(TEST_DIR)/image_write_test.c | $(BUILD_DIR)
	$(CC) $(INCLUDES) $(TEST_CFLAGS) -DIWT_TEST $< -lm $(TEST_LDFLAGS) -o $@

$(BUILD_DIR)/image_fuzzer_standalone: $(TEST_DIR)/fuzz_main.c $(TEST_DIR)/stbi_read_fuzzer.c | $(BUILD_DIR)
	$(CC) $(INCLUDES) $(TEST_CFLAGS) $^ -lm $(TEST_LDFLAGS) -o $@

$(BUILD_DIR)/stbi_read_fuzzer: $(TEST_DIR)/stbi_read_fuzzer.c | $(BUILD_DIR)
	$(CC) $(INCLUDES) $(FUZZ_CFLAGS) $< -lm $(FUZZ_LDFLAGS) -o $@

$(BUILD_DIR)/stbtt_read_fuzzer: $(TEST_DIR)/stbtt_read_fuzzer.c | $(BUILD_DIR)
	$(CC) $(INCLUDES) $(FUZZ_CFLAGS) $< -lm $(FUZZ_LDFLAGS) -o $@

$(BUILD_DIR)/stbtt_fuzzer_standalone: $(TEST_DIR)/fuzz_main.c $(TEST_DIR)/stbtt_read_fuzzer.c | $(BUILD_DIR)
	$(CC) $(INCLUDES) $(TEST_CFLAGS) $^ -lm $(TEST_LDFLAGS) -o $@

$(BUILD_DIR)/stbiw_write_fuzzer: $(TEST_DIR)/stbiw_write_fuzzer.c | $(BUILD_DIR)
	$(CC) $(INCLUDES) $(FUZZ_CFLAGS) $< -lm $(FUZZ_LDFLAGS) -o $@

$(BUILD_DIR)/stbiw_write_fuzzer_standalone: $(TEST_DIR)/fuzz_main.c $(TEST_DIR)/stbiw_write_fuzzer.c | $(BUILD_DIR)
	$(CC) $(INCLUDES) $(TEST_CFLAGS) $^ -lm $(TEST_LDFLAGS) -o $@

$(BUILD_DIR)/hexwave_fuzzer: $(TEST_DIR)/hexwave_fuzzer.c | $(BUILD_DIR)
	$(CC) $(INCLUDES) $(FUZZ_CFLAGS) $< -lm $(FUZZ_LDFLAGS) -o $@

$(BUILD_DIR)/hexwave_fuzzer_standalone: $(TEST_DIR)/fuzz_main.c $(TEST_DIR)/hexwave_fuzzer.c | $(BUILD_DIR)
	$(CC) $(INCLUDES) $(TEST_CFLAGS) $^ -lm $(TEST_LDFLAGS) -o $@

$(BUILD_DIR)/stb_easy_font_fuzzer: $(TEST_DIR)/stb_easy_font_fuzzer.c | $(BUILD_DIR)
	$(CC) $(INCLUDES) $(FUZZ_CFLAGS) $< -lm $(FUZZ_LDFLAGS) -o $@

$(BUILD_DIR)/stb_easy_font_fuzzer_standalone: $(TEST_DIR)/fuzz_main.c $(TEST_DIR)/stb_easy_font_fuzzer.c | $(BUILD_DIR)
	$(CC) $(INCLUDES) $(TEST_CFLAGS) $^ -lm $(TEST_LDFLAGS) -o $@

$(BUILD_DIR)/stb_c_lexer_fuzzer: $(TEST_DIR)/stb_c_lexer_fuzzer.c | $(BUILD_DIR)
	$(CC) $(INCLUDES) $(FUZZ_CFLAGS) $< -lm $(FUZZ_LDFLAGS) -o $@

$(BUILD_DIR)/stb_c_lexer_fuzzer_standalone: $(TEST_DIR)/fuzz_main.c $(TEST_DIR)/stb_c_lexer_fuzzer.c | $(BUILD_DIR)
	$(CC) $(INCLUDES) $(TEST_CFLAGS) $^ -lm $(TEST_LDFLAGS) -o $@

$(BUILD_DIR)/bug_%: $(TEST_DIR)/bug_%.c | $(BUILD_DIR)
	$(CC) $(INCLUDES) $(TEST_CFLAGS) $< -lm $(TEST_LDFLAGS) -o $@

bug-tests: $(BUG_TEST_BINS)
	@if [ -z "$(BUG_TEST_SRCS)" ]; then \
		printf '%s\n' 'No hardening regression tests found in tests/bug_*.c'; \
	else \
		for test_bin in $(BUG_TEST_BINS); do \
			printf 'running %s\n' "$$test_bin"; \
			$(RUN_ENV) "$$test_bin"; \
		done; \
	fi

fuzzers: $(BUILD_DIR)/image_fuzzer_standalone $(BUILD_DIR)/stbi_read_fuzzer $(BUILD_DIR)/stbtt_read_fuzzer $(BUILD_DIR)/stbtt_fuzzer_standalone $(BUILD_DIR)/stbiw_write_fuzzer_standalone $(BUILD_DIR)/stbiw_write_fuzzer $(BUILD_DIR)/hexwave_fuzzer $(BUILD_DIR)/hexwave_fuzzer_standalone $(BUILD_DIR)/stb_easy_font_fuzzer $(BUILD_DIR)/stb_easy_font_fuzzer_standalone $(BUILD_DIR)/stb_c_lexer_fuzzer $(BUILD_DIR)/stb_c_lexer_fuzzer_standalone

fuzz-smoke: fuzzers
	$(FUZZ_RUN_ENV) $(BUILD_DIR)/stbi_read_fuzzer -runs=1 -artifact_prefix=$(BUILD_DIR)/ $(TEST_DIR)/pngsuite/primary_check
	$(FUZZ_RUN_ENV) $(BUILD_DIR)/stbtt_read_fuzzer -runs=1 -artifact_prefix=$(BUILD_DIR)/

clean:
	rm -rf $(BUILD_DIR)
	rm -f a.out image_fuzzer image_write_test
