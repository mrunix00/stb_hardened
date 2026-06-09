#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

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
    const char *svg;
    size_t off;

    /* Build a minimal TrueType font (290 bytes) with 8 tables including SVG */
    font = (unsigned char *)calloc(290, 1);
    if (!font) return 1;

    put_u32(font, 0, 0x00010000);
    put_u16(font, 4, 8);
    put_u16(font, 6, 64);
    put_u16(font, 8, 2);
    put_u16(font, 10, 48);

    off = 12 + 16 * 8;

    /* cmap (index 0) — format 6, maps codepoint 0 to glyph 0 */
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

    /* maxp (index 4) */
    {
        size_t maxp = off;
        add_table(font, 4, "maxp", (unsigned long)maxp, 6);
        put_u16(font, maxp + 4, 1);
        off = maxp + 6;
    }

    /* loca (index 5) */
    size_t loca = off;
    add_table(font, 5, "loca", (unsigned long)loca, 8);
    off = loca + 8;

    /* glyf (index 6) — empty glyph */
    {
        size_t glyf = off;
        add_table(font, 6, "glyf", (unsigned long)glyf, 10);
        put_u16(font, glyf, 0);
        put_u32(font, loca, 0);
        put_u32(font, loca + 4, 10);
        off = glyf + 10;
    }

    /* SVG (index 7) — header + document list with huge numEntries */
    {
        size_t svg = off;
        add_table(font, 7, "SVG ", (unsigned long)svg, 10);
        put_u16(font, svg, 0);          /* version */
        put_u32(font, svg + 2, 8);      /* offset to doc list = 8 (right after header) */
        put_u16(font, svg + 8, 0x7FFF); /* numEntries = huge */
        /* svg_docs starts at svg + 10 → past the 290-byte buffer → OOB */
        off = svg + 10;
    }

    if (!stbtt_InitFontEx(&info, font, 0, 290)) {
        fprintf(stderr, "FAIL: stbtt_InitFont failed\n");
        free(font);
        return 1;
    }

    /* Trigger OOB read in stbtt_FindSVGDoc via stbtt_GetGlyphSVG */
    svg = NULL;
    stbtt_GetGlyphSVG(&info, 0, &svg);
    /* If we reach here, no crash — the fix works */
    fprintf(stderr, "PASS: stbtt_GetGlyphSVG returned (no crash)\n");
    free(font);
    return 0;
}
