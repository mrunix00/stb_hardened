#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STBTT_assert(x) assert(x)
#define STB_TRUETYPE_IMPLEMENTATION
#include "../stb_truetype.h"

/*
 * Regression test for BUG-stb_truetype-030.
 *
 * stbtt__GetGlyfOffsetEnd (stb_truetype.h:1752-1759) computes the glyph
 * end offset from the loca table and returns it WITHOUT validating against
 * info->data_len.  Its sibling stbtt__GetGlyfOffset (fixed by BUG-022)
 * validates both g1 AND g2 against data_len, but the End variant was
 * overlooked.  The result is that stbtt__GetGlyphShapeTT sees a bogus
 * glyph_end_ptr = data + huge_glyph_end, defeating the later bounds checks.
 *
 * We build a minimal TrueType font with a glyph whose loca end offset
 * points far past the allocated buffer.  Using stbtt_InitFontEx with
 * the real data_len, we call stbtt__GetGlyfOffsetEnd directly:
 *
 *   Unpatched: returns the out-of-bounds offset (e.g. 131370).
 *   Patched:   returns -1 because g2 > data_len.
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
static void put_tag(uint8_t *p, size_t dir_off, const char *t) {
   p[dir_off]   = (uint8_t)t[0];
   p[dir_off+1] = (uint8_t)t[1];
   p[dir_off+2] = (uint8_t)t[2];
   p[dir_off+3] = (uint8_t)t[3];
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

   /* Offset table: 7 tables, directory at offset 12. */
   put_u32(font, 0, 0x00010000);
   put_u16(font, 4, 7);
   put_u16(font, 6, 128);
   put_u16(font, 8, 3);
   put_u16(font, 10, 0);

   put_dir(font, 12,  "cmap", 0, 128, 32);
   put_dir(font, 28,  "head", 0, 160, 54);
   put_dir(font, 44,  "hhea", 0, 214, 36);
   put_dir(font, 60,  "hmtx", 0, 250, 8);
   put_dir(font, 76,  "maxp", 0, 258, 6);
   put_dir(font, 92,  "loca", 0, 264, 4);
   put_dir(font, 108, "glyf", 0, 300, 20);

   /* cmap: minimal Format 4. */
   put_u16(font, 128, 0);
   put_u16(font, 130, 1);
   put_u16(font, 132, 0);
   put_u16(font, 134, 3);
   put_u32(font, 136, 140);
   put_u16(font, 140, 4);
   put_u16(font, 142, 0);
   put_u16(font, 144, 0);
   put_u16(font, 146, 0);

   /* head at 160. */
   put_u32(font, 160, 0x00010000);
   put_u32(font, 164, 0x00005000);
   put_u32(font, 168, 0);
   put_u32(font, 172, 0x5F0F3CF5);
   put_u16(font, 176, 0x000B);
   put_u16(font, 178, 1000);
   put_u16(font, 180, 0);      /* indexToLocFormat = 0 (short) */

   /* hhea at 214. */
   put_u32(font, 214, 0x00010000);
   put_u16(font, 218, 800);
   put_u16(font, 220, -200);

   /* maxp at 258. numGlyphs = 1. */
   put_u32(font, 258, 0x00005000);
   put_u16(font, 262, 1);

   /*
    * loca at 264, 4 bytes (2 short entries for 1 glyph).
    * loca[0] = 0   → g1 = glyf + 0     = 300 (within buffer)
    * loca[1] = 0xFFFF → g2 = glyf + 0xFFFF*2 = 300 + 131070 = 131370 (past buffer)
    */
   put_u16(font, 264, 0);
   put_u16(font, 266, 0xFFFF);

   /* glyf at 300. */
   put_u16(font, 300, 0);   /* numberOfContours = 0 (empty glyph) */

   stbtt_fontinfo info;
   /*
    * Use InitFontEx with the real data_len so that the BUG-030 bounds
    * check (if present) can fire.  BUG-022 also checks g2 in
    * stbtt__GetGlyfOffset, so we test stbtt__GetGlyfOffsetEnd directly.
    */
   int rc = stbtt_InitFontEx(&info, font, 0, font_size);
   if (rc == 0) {
      fprintf(stderr, "InitFont failed unexpectedly\n");
      free(font);
      return 2;
   }

   /* Call stbtt__GetGlyfOffsetEnd directly. */
   int end = stbtt__GetGlyfOffsetEnd(&info, 0);
   if (end >= 0) {
      /* Unpatched: end = 131370 (past buffer) */
      fprintf(stderr, "FAIL: stbtt__GetGlyfOffsetEnd returned %d (expected -1)\n", end);
      free(font);
      return 1;
   }

   printf("PASS: stbtt__GetGlyfOffsetEnd returned -1 (no crash)\n");
   free(font);
   return 0;
}
