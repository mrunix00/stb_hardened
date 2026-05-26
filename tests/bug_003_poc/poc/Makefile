STB_DIR ?= ./stb
CFLAGS  ?= -std=c99 -O0 -g -Wall -Wextra
ASAN    ?= -fsanitize=address -fno-omit-frame-pointer

all: stb_composite_cycle_harness

stb_composite_cycle_harness: stb_composite_cycle_harness.c
	$(CC) $(ASAN) $(CFLAGS) -I$(STB_DIR) $< -lm -o $@

run: all
	@echo "--- negative control (mode=empty) ---"
	./stb_composite_cycle_harness empty
	@echo "--- positive (mode=cycle) — expect AddressSanitizer stack-overflow, exit 134 ---"
	-ASAN_OPTIONS=abort_on_error=1:symbolize=1 timeout 5s ./stb_composite_cycle_harness cycle ; \
	  echo "exit=$$?"

clean:
	rm -f stb_composite_cycle_harness

.PHONY: all run clean
