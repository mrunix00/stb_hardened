#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "../stb_truetype.h"

/*
 * Regression test for BUG-stb_truetype-021.
 *
 * stbtt_GetGlyphBitmapSubpixel (stb_truetype.h:3831) computes the
 * glyph bitmap dimensions w and h (both int) and allocates
 *   STBTT_malloc(w * h, ...)
 * If w and h are both large (e.g. after a huge `scale`), `w * h` is
 * `int * int` and wraps to a small positive value (or fails the
 * malloc with allocation-size-too-big). The fix caps each
 * dimension at 0xffff and does the multiplication in size_t.
 *
 * This test exercises the cap by building a minimal font with a
 * non-empty glyph (1 line contour from (0,0) to (1000,1000) font
 * units) and asking for a 1000x scale, which makes the requested
 * bitmap ~1M x 1M pixels. The patched library returns NULL; the
 * unpatched library attempts to allocate ~1 trillion bytes and
 * ASan aborts.
 */

static void put_u8(uint8_t *p, size_t off, uint8_t v) { p[off] = v; }
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

   /* Runtime-initialized buffer (so the compiler doesn't
    * constant-fold). */
   for (int i = 0; i < font_size; i++) font[i] = 0;
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
   put_dir(font, 108, "glyf", 0, 268, 20);

   /* cmap: minimal Format 4. */
   put_u16(font, 128, 0);
   put_u16(font, 130, 1);
   put_u16(font, 132, 0);
   put_u16(font, 134, 3);
   put_u32(font, 136, 140);
   put_u16(font, 140, 4);
   put_u16(font, 142, 0);
   put_u16(font, 146, 0);

   /* head. unitsPerEm = 1000, indexToLocFormat = 0 (short). */
   put_u32(font, 160, 0x00010000);
   put_u32(font, 164, 0x00005000);
   put_u32(font, 168, 0);
   put_u32(font, 172, 0x5F0F3CF5);
   put_u16(font, 176, 0x000B);
   put_u16(font, 178, 1000);
   put_u16(font, 180, 0);

   /* hhea. ascender = 1, descender = -1 (non-zero fheight). */
   put_u32(font, 214, 0x00010000);
   put_u16(font, 218, 1);
   put_u16(font, 220, 0xFFFFu);   /* -1 as u16 */

   /* maxp. numGlyphs = 1. */
   put_u32(font, 258, 0x00005000);
   put_u16(font, 262, 1);

   /* loca. Short format. glyph 0 spans [0, 19] bytes in glyf. */
   put_u16(font, 264, 0);
   put_u16(font, 266, 19);

   /* glyf. Simple glyph with 1 line contour from (0,0) to
    * (1000,1000) font units. */
   int off = 268;
   put_u16(font, off, 1); off += 2;        /* numberOfContours = 1 */
   put_u16(font, off, 0); off += 2;        /* xMin */
   put_u16(font, off, 0); off += 2;        /* yMin */
   put_u16(font, off, 1000); off += 2;     /* xMax */
   put_u16(font, off, 1000); off += 2;     /* yMax */
   put_u16(font, off, 0); off += 2;        /* endPtsOfContours[0] = 0 */
   put_u16(font, off, 0); off += 2;        /* instructionLength = 0 */
   /* One vertex: line. flags = STBTT_vline (1). The line is from
    * (0,0) to (1000,1000) font units. In the on-curve contour, we
    * only need ONE line vertex (the start of the line). stbtt adds
    * the closing segment implicitly. */
   put_u8(font, off, 1); off += 1;         /* flags[0] = STBTT_vline */
   /* For x: flag & 2 = 0 → 2-byte x-coord (absolute since flag & 16 = 0). */
   put_u16(font, off, 1000); off += 2;     /* x[0] = 1000 */
   /* For y: flag & 4 = 0 → 2-byte y-coord. */
   put_u16(font, off, 1000); off += 2;     /* y[0] = 1000 */

   stbtt_fontinfo info;
   int rc = stbtt_InitFontEx(&info, font, 0, font_size);
   if (rc == 0) {
      fprintf(stderr, "InitFont failed unexpectedly\n");
      free(font);
      return 2;
   }

   /* Request a scale that would push w or h past 0xffff. The bbox
    * is 1000x1000, so scale 1000 yields ~1M x 1M pixels. */
   float scale = 1000.0f;
   int w = -1, h = -1, xoff = 0, yoff = 0;
   unsigned char *bitmap = stbtt_GetGlyphBitmapSubpixel(&info, scale, scale, 0, 0, 0, &w, &h, &xoff, &yoff);
   printf("bitmap=%p w=%d h=%d\n", (void*)bitmap, w, h);
   if (bitmap) {
      fprintf(stderr, "expected NULL for huge bitmap, got %p (%d x %d)\n",
              (void*)bitmap, w, h);
      stbtt_FreeBitmap(bitmap, info.userdata);
      free(font);
      return 1;
   }
   free(font);
   return 0;
}
