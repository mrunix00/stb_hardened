/*
 * Regression test for BUG-stb_truetype-028:
 *   The BUG-027 fix for stbtt__buf_skip used an `if (o < 0)` branch
 *   that computed `-o`, which is undefined behavior for o == INT_MIN
 *   (-2147483648). An attacker-controlled CFF count that ends up as
 *   INT_MIN after some intermediate arithmetic triggers UBSan to
 *   abort with `negation of -2147483648 cannot be represented in
 *   type 'int'`.
 *
 * Crash input: 1196 bytes total (1-byte mode prefix + 1195-byte CFF
 * payload). The CFF table index has a count whose skip arithmetic
 * produces INT_MIN as the offset to skip.
 *
 * The test loads tests/bug_stb_truetype_028.bin and calls
 * stbtt_InitFontEx on the payload. After the fix, the unsigned
 * addition in stbtt__buf_skip handles all int values (including
 * INT_MIN) without invoking the unary minus on INT_MIN.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

int main(void)
{
    FILE *f = fopen("tests/bug_stb_truetype_028.bin", "rb");
    if (!f) {
        fprintf(stderr, "could not open tests/bug_stb_truetype_028.bin\n");
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 8) {
        fclose(f);
        fprintf(stderr, "input too small\n");
        return 1;
    }

    unsigned char *buf = (unsigned char *) malloc((size_t) sz);
    if (!buf) { fclose(f); return 1; }
    if (fread(buf, 1, (size_t) sz, f) != (size_t) sz) {
        fclose(f); free(buf); return 1;
    }
    fclose(f);

    const unsigned char *payload = buf + 1; /* skip fuzzer mode byte */
    int psize = (int) sz - 1;

    stbtt_fontinfo info;
    memset(&info, 0, sizeof(info));
    int rc = stbtt_InitFontEx(&info, payload, 0, psize);
    (void) rc;

    free(buf);
    return 0;
}
