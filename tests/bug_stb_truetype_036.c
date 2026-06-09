#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

int main(void) {
    unsigned char *data;
    stbtt_fontinfo info;
    int x0, y0, x1, y1;

    /* 50-byte buffer; info.head = 40 makes head+36 = 76 (OOB) on unpatched */
    data = (unsigned char *)calloc(50, 1);
    if (!data) return 1;

    info.data = data;
    info.fontstart = 0;
    info.data_len = 0;
    info.head = 40;

    stbtt_GetFontBoundingBox(&info, &x0, &y0, &x1, &y1);

    fprintf(stderr, "PASS: box=%d,%d,%d,%d\n", x0, y0, x1, y1);
    free(data);
    return 0;
}
