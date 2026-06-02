#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STBTT_assert(x) assert(x)
#define STB_TRUETYPE_IMPLEMENTATION
#include "../stb_truetype.h"

/*
 * Regression test for BUG-stb_truetype-018.
 *
 * stbtt__cff_index_get (stb_truetype.h:1261) returns
 *   stbtt__buf_range(&b, 2+(count+1)*offsize+start, end - start)
 * where start and end are font-controlled `int` values read from
 * the CFF index offsets. The `end - start` is signed subtraction
 * with attacker-controlled operands. If end < start (or one
 * value has its high bit set in 4-byte mode), the subtraction
 * is undefined behavior in C and UBSan fires with
 *   signed integer overflow: X - Y cannot be represented in type 'int'
 *
 * The test replays the two saved crash artifacts
 * (`crash-cff-index-get-overflow.bin`,
 * `crash-cff-index-get-overflow-2.bin`, now in
 * `tests/bug_stb_truetype_018{,.b}.bin`) through stbtt_InitFontEx.
 * Note that on the fully-unpatched library these inputs also trip
 * BUG-017 first (the offsize assert); however, after BUG-017 is
 * fixed the same inputs still reach the `end - start` path, and
 * UBSan will report the BUG-018 overflow if the early-return
 * guard is missing.
 *
 * Unpatched: the process aborts (either in BUG-017's offsize
 *   check, or — with only BUG-018 reverted — in UBSan's
 *   signed-integer-overflow report).
 * Patched:   exit 0, no sanitizer output.
 */

static int replay(const char *path) {
   FILE *f = fopen(path, "rb");
   if (!f) {
      fprintf(stderr, "could not open %s\n", path);
      return 1;
   }
   fseek(f, 0, SEEK_END);
   long siz = ftell(f);
   rewind(f);
   if (siz < 1) { fclose(f); return 1; }
   uint8_t *buf = (uint8_t *)malloc((size_t)siz);
   if (!buf) { fclose(f); return 1; }
   if (fread(buf, (size_t)siz, 1, f) != 1) {
      fclose(f); free(buf); return 1;
   }
   fclose(f);
   /* Skip the fuzzer's 1-byte mode prefix. */
   stbtt_fontinfo info;
   (void)stbtt_InitFontEx(&info, buf + 1, 0, (int)siz - 1);
   free(buf);
   return 0;
}

int main(void) {
   if (replay("bug_stb_truetype_018.bin")  != 0) return 1;
   if (replay("bug_stb_truetype_018b.bin") != 0) return 1;
   return 0;
}
