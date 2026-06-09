#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

/* Build an OTTO font whose CFF table directory entry claims a size
   >= 0x40000000 (1 GB), triggering the assert in stbtt__new_buf. */

static void put_u16(unsigned char *p, size_t off, unsigned short v) {
    p[off]     = (unsigned char)(v >> 8);
    p[off + 1] = (unsigned char)(v & 0xff);
}

static void put_u32(unsigned char *p, size_t off, unsigned long v) {
    p[off]     = (unsigned char)(v >> 24);
    p[off + 1] = (unsigned char)(v >> 16);
    p[off + 2] = (unsigned char)(v >> 8);
    p[off + 3] = (unsigned char)(v & 0xff);
}

static void add_table(unsigned char *p, size_t index, const char tag[4],
                      unsigned long off, unsigned long len) {
    size_t dir = 12 + 16 * index;
    memcpy(p + dir, tag, 4);
    put_u32(p, dir + 8, off);
    put_u32(p, dir + 12, len);
}

int main(void) {
    unsigned char *font;
    stbtt_fontinfo info;
    size_t off;

    /* Allocate a ~220-byte buffer */
    font = (unsigned char *)calloc(256, 1);
    if (!font) return 1;

    /* OTTO signature */
    font[0] = 'O'; font[1] = 'T'; font[2] = 'T'; font[3] = 'O';
    put_u16(font, 4, 5);            /* 5 tables */
    put_u16(font, 6, 64);
    put_u16(font, 8, 2);
    put_u16(font, 10, 48);

    off = 12 + 16 * 5;              /* directory = 92 bytes */

    /* cmap (index 0) — format 6 */
    {
        size_t cmap = off;
        add_table(font, 0, "cmap", (unsigned long)cmap, 22);
        put_u16(font, cmap, 0);
        put_u16(font, cmap + 2, 1);
        put_u16(font, cmap + 4, 3);
        put_u16(font, cmap + 6, 1);
        put_u32(font, cmap + 8, 12);
        put_u16(font, cmap + 12, 6);
        put_u16(font, cmap + 14, 10);
        put_u16(font, cmap + 20, 1);
        off = cmap + 22;
    }

    /* head (index 1) */
    {
        size_t head = off;
        add_table(font, 1, "head", (unsigned long)head, 54);
        put_u16(font, head + 18, 1024);
        put_u16(font, head + 50, 1);
        off = head + 54;
    }

    /* hhea (index 2) */
    {
        size_t hhea = off;
        add_table(font, 2, "hhea", (unsigned long)hhea, 36);
        put_u16(font, hhea + 34, 1);
        off = hhea + 36;
    }

    /* hmtx (index 3) */
    {
        size_t hmtx = off;
        add_table(font, 3, "hmtx", (unsigned long)hmtx, 4);
        off = hmtx + 4;
    }

    /* CFF (index 4) — directory entry claims size >= 0x40000000 */
    {
        size_t cff = off;
        /* Declare a CFF table with huge size to trigger assert */
        add_table(font, 4, "CFF ", (unsigned long)cff, 0x40000001);
        /* Minimal CFF header at offset (not needed - assert fires first) */
        off = cff + 2;
    }

    /* Use stbtt_InitFont (legacy, data_len=0) to trigger the bug */
    if (!stbtt_InitFont(&info, font, 0)) {
        /* Patched: table size capped, InitFont fails gracefully */
        fprintf(stderr, "PASS: stbtt_InitFont returned 0 (no assertion)\n");
        free(font);
        return 0;
    }

    /* Should not reach here on unpatched (assert aborts) */
    fprintf(stderr, "PASS: no assertion? unexpected success\n");
    free(font);
    return 0;
}
