#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

int main() {
    int font_len = 200;
    unsigned char *font = malloc(font_len);
    memset(font, 0, font_len);

    // OTTO header
    font[0] = 'O'; font[1] = 'T'; font[2] = 'T'; font[3] = 'O';
    font[5] = 5; // numTables

    // CFF Table record
    memcpy(font + 12, "CFF ", 4);
    font[12+11] = 92; // offset
    font[12+15] = 200 - 92; // length

    // Required tables
    memcpy(font + 12 + 16, "cmap", 4);
    font[12+16+11] = 180; font[12+16+15] = 4;
    memcpy(font + 12 + 32, "head", 4);
    font[12+32+11] = 180; font[12+32+15] = 4;
    memcpy(font + 12 + 48, "hhea", 4);
    font[12+48+11] = 180; font[12+48+15] = 4;
    memcpy(font + 12 + 64, "hmtx", 4);
    font[12+64+11] = 180; font[12+64+15] = 4;

    // CFF table data at 92
    unsigned char *cff = font + 92;
    cff[0] = 1; cff[1] = 0; cff[2] = 4; cff[3] = 1; // major, minor, hdrSize, offSize

    // name INDEX
    cff[4] = 0; cff[5] = 1; // count = 1
    cff[6] = 1;             // offSize = 1
    cff[7] = 1; cff[8] = 2; // offsets
    cff[9] = 'A';           // name

    // topdict INDEX
    cff[10] = 0; cff[11] = 1; // count = 1
    cff[12] = 1;              // offSize = 1
    cff[13] = 1; cff[14] = 3; // offsets. Data is 2 bytes.
    // topdict data: Trigger BUG-stb_truetype-008
    cff[15] = 255; // Invalid operand byte for stbtt__cff_int (default case triggers STBTT_assert(0))
    cff[16] = 17;  // Key (doesn't matter)

    stbtt_fontinfo info;
    // We expect this to trigger an assertion failure
    stbtt_InitFont(&info, font, 0);

    free(font);
    return 0;
}
