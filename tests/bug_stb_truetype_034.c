#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

/* Minimal font (12 bytes, heap) with valid signature and zero tables.
   stbtt_FindMatchingFontEx(flags != 0) → stbtt__matches → stbtt__find_table
   returns 0 for "head", then ttUSHORT(fc+44) reads 33 bytes past the
   12-byte allocation. */

int main(void) {
    unsigned char *font = (unsigned char *)calloc(12, 1);
    int ret;
    if (!font) return 1;

    font[0] = 0x00; font[1] = 0x01; font[2] = 0x00; font[3] = 0x00;
    font[4] = 0x00; font[5] = 0x00;  /* numTables = 0 */

    ret = stbtt_FindMatchingFontEx(font, 12, "test", 1);

    /* Patched: stbtt__matches returns 0 before OOB read */
    fprintf(stderr, "PASS: returned %d (no crash)\n", ret);
    free(font);
    return 0;
}
