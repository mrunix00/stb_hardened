/*
 * Regression test for BUG-stb_truetype-025:
 *   stbtt_GetNumberOfFonts reads the TTC version/numFonts fields without
 *   checking that the buffer is large enough to hold the 12-byte TTC
 *   header. A 7-byte input that starts with "ttcf" causes a heap-buffer-
 *   overflow read in ttULONG.
 *
 * Crash input (8 bytes total, 1-byte mode prefix + 7-byte "ttcf" payload):
 *   0x55  0x74 0x74 0x63 0x66  0x00 0x26 0x00
 *
 * The 7-byte payload is "ttcf\x00&\x00", so stbtt_tag(font, "ttcf")
 * returns true. The internal function then reads 4 bytes at offset 4
 * (the version) and 4 more at offset 8 (the numFonts) without bounds
 * checking. With a 7-byte buffer, the read at offset 4-7 spills past
 * the end.
 *
 * The test exercises both stbtt_GetNumberOfFonts (legacy, no length)
 * and stbtt_GetNumberOfFontsEx (with the real length). Neither should
 * produce an ASan/UBSan fault after the fix.
 */

#include <stdio.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

/* Mode byte + 7-byte font payload, no extra padding. */
static const unsigned char kFullInput[8] = {
    0x55,
    0x74, 0x74, 0x63, 0x66,
    0x00, 0x26, 0x00
};

int main(void)
{
    const unsigned char *data = kFullInput;
    int size = (int) sizeof(kFullInput);

    int n1 = stbtt_GetNumberOfFonts(data);
    int n2 = stbtt_GetNumberOfFontsEx(data, size);
    (void) n1; (void) n2;

    return 0;
}
