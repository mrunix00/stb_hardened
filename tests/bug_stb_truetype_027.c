/*
 * Regression test for BUG-stb_truetype-027:
 *   stbtt__buf_skip adds the skip amount to b->cursor using signed
 *   int arithmetic. An attacker-controlled skip amount (e.g. a CFF
 *   count * offsize product, or a result of stbtt__buf_get returning
 *   0x7fffffff and then subtracting 1) can overflow the int addition
 *   and trigger UBSan's signed-integer-overflow trap.
 *
 * Crash input: 670 bytes total (1-byte mode prefix + 669-byte CFF
 * payload). The payload contains an OTTO font whose CFF table index
 * has a large count value, so stbtt__cff_get_index computes a huge
 * skip and stbtt__buf_skip(b, INT_MAX-ish) overflows.
 *
 * The test loads tests/bug_stb_truetype_027.bin and calls
 * stbtt_InitFontEx on the payload. After the fix, the skip is done
 * with unsigned arithmetic and the cursor is clamped to b->size.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

int main(void)
{
    FILE *f = fopen("tests/bug_stb_truetype_027.bin", "rb");
    if (!f) {
        fprintf(stderr, "could not open tests/bug_stb_truetype_027.bin\n");
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
