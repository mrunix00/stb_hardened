/*
 * Regression test for BUG-stb_truetype-026:
 *   stbtt__get_table_size returns the attacker-controlled directory
 *   length without capping by the actual buffer extent. The caller in
 *   stbtt_InitFont_internal (and stbtt__buf_range consumers elsewhere)
 *   then constructs a stbtt__buf whose size exceeds the heap allocation,
 *   and the subsequent stbtt__cff_get_index → stbtt__buf_get8 walks
 *   past the buffer.
 *
 * Crash input: 295 bytes total (1-byte mode prefix + 294-byte OTTO
 * font). The CFF table directory entry claims a length of 0x00ec0000
 * (~15 MB) but the actual buffer is 294 bytes, so the stbtt__buf
 * wraps a much larger "size" than the underlying allocation.
 *
 * The test loads tests/bug_stb_truetype_026.bin (saved crash input) and
 * calls stbtt_InitFontEx on the 294-byte payload. With the fix in
 * place, the CFF buffer is capped to the actual data extent and no
 * OOB read or assert fires.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

int main(void)
{
    FILE *f = fopen("tests/bug_stb_truetype_026.bin", "rb");
    if (!f) {
        fprintf(stderr, "could not open tests/bug_stb_truetype_026.bin\n");
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
