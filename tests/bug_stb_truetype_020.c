#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "../stb_truetype.h"

/*
 * Regression test for BUG-stb_truetype-020.
 *
 * stbtt_ScaleForPixelHeight (stb_truetype.h:2773) computes
 *   fheight = ttSHORT(data+hhea+4) - ttSHORT(data+hhea+6);
 *   return (float)height / fheight;
 * A font whose hhea table sets ascender == descender (e.g. both 0,
 * or both the same value) makes fheight == 0, and the float
 * division by zero is undefined behavior. UBSan flags it as
 * `division by zero` in -fsanitize=float-divide-by-zero.
 *
 * We synthesize a minimal valid font whose hhea table has
 * ascender = descender = 0. InitFont succeeds because the
 * directory is bounded by the BUG-014 fix, the required tables
 * (cmap, head, hhea, hmtx) are present.
 *
 * Unpatched: stbtt_ScaleForPixelHeight returns +inf (16.0 / 0.0)
 * because the float div-by-zero is not trapped by default under
 * `-fsanitize=undefined`. The test fails because the value is not
 * finite.
 * Patched:   the `if (fheight == 0) fheight = 1` guard makes the
 * function return height/1 == 16.0, which is finite.
 */

static void put_u16(uint8_t *p, size_t off, uint16_t v) {
   p[off]   = (uint8_t)(v >> 8);
   p[off+1] = (uint8_t)(v & 0xff);
}
static void put_u32(uint8_t *p, size_t off, uint32_t v) {
   p[off]   = (uint8_t)(v >> 24);
   p[off+1] = (uint8_t)(v >> 16);
   p[off+2] = (uint8_t)(v >> 8);
   p[off+3] = (uint8_t)(v & 0xff);
}
static void put_tag(uint8_t *p, size_t off, const char *t) {
   p[off]   = (uint8_t)t[0];
   p[off+1] = (uint8_t)t[1];
   p[off+2] = (uint8_t)t[2];
   p[off+3] = (uint8_t)t[3];
}
static void put_dir(uint8_t *p, size_t dir_off, const char *tag, uint32_t checksum, uint32_t offset, uint32_t length) {
   put_tag(p, dir_off+0, tag);
   put_u32(p, dir_off+4, checksum);
   put_u32(p, dir_off+8, offset);
   put_u32(p, dir_off+12, length);
}

int main(void) {
   enum { font_size = 320 };
   uint8_t *font = (uint8_t *)calloc(1, font_size);
   if (!font) return 2;

   /* Offset table: 7 tables. */
   put_u32(font, 0, 0x00010000);
   put_u16(font, 4, 7);
   put_u16(font, 6, 128);
   put_u16(font, 8, 3);
   put_u16(font, 10, 0);

   /* Layout: directory 12..123 (7 entries × 16), tables start at 128. */
   /* cmap  128, 32 bytes */
   /* head  160, 54 bytes */
   /* hhea  214, 36 bytes — ascender=0, descender=0 */
   /* hmtx  250, 8  bytes */
   /* maxp  258, 6  bytes — numGlyphs=1 */
   /* loca  264, 4  bytes — 1 glyph: [0, 0] */
   /* glyf  268, 0  bytes */

   put_dir(font, 12,  "cmap", 0, 128, 32);
   put_dir(font, 28,  "head", 0, 160, 54);
   put_dir(font, 44,  "hhea", 0, 214, 36);
   put_dir(font, 60,  "hmtx", 0, 250, 8);
   put_dir(font, 76,  "maxp", 0, 258, 6);
   put_dir(font, 92,  "loca", 0, 264, 4);
   put_dir(font, 108, "glyf", 0, 268, 0);

   /* cmap: minimal valid Format 4 (empty). */
   put_u16(font, 128, 0);    /* version */
   put_u16(font, 130, 1);    /* numTables */
   put_u16(font, 132, 0);    /* platformID (Unicode) */
   put_u16(font, 134, 3);    /* encodingID (BMP) */
   put_u32(font, 136, 140);  /* subtable offset */
   put_u16(font, 140, 4);    /* format = 4 */
   put_u16(font, 142, 0);    /* length */
   put_u16(font, 144, 0);    /* language */
   put_u16(font, 146, 0);    /* segCountX2 */

   /* head at 160. unitsPerEm non-zero. */
   put_u32(font, 160, 0x00010000);
   put_u32(font, 164, 0x00005000);
   put_u32(font, 168, 0);
   put_u32(font, 172, 0x5F0F3CF5);
   put_u16(font, 176, 0x000B);
   put_u16(font, 178, 1000);   /* unitsPerEm */
   put_u16(font, 180, 0);      /* indexToLocFormat = 0 (short) */

   /* hhea at 214, 36 bytes. ascender = descender = 0. */
   put_u32(font, 214, 0x00010000);
   put_u16(font, 218, 0);   /* ascender  */
   put_u16(font, 220, 0);   /* descender */

   /* maxp at 258, 6 bytes. numGlyphs=1. */
   put_u32(font, 258, 0x00005000);
   put_u16(font, 262, 1);   /* numGlyphs */

   /* loca at 264, 4 bytes. indexToLocFormat=0 (short), so 1 glyph =
    * 2 entries × 2 bytes = 4 bytes. Both 0 → empty glyph. */
   put_u16(font, 264, 0);
   put_u16(font, 266, 0);

   stbtt_fontinfo info;
   int rc = stbtt_InitFontEx(&info, font, 0, font_size);
   if (rc == 0) {
      fprintf(stderr, "InitFont failed unexpectedly\n");
      free(font);
      return 2;
   }
   float s = stbtt_ScaleForPixelHeight(&info, 16.0f);
   printf("scale=%g\n", (double)s);
   free(font);
   if (s != s) return 1;  /* NaN */
   if (!(s == s)) return 1; /* NaN via different idiom */
   if (s > 1e30f || s < -1e30f) return 1; /* inf */
   if (s <= 0.0f) return 1; /* degenerate */
   return 0;
}
