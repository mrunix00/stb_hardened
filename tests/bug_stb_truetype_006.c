#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "../stb_truetype.h"

static void put_u16(uint8_t *p, size_t off, uint16_t v) {
   p[off] = (uint8_t)(v >> 8);
   p[off + 1] = (uint8_t)(v & 0xff);
}

static void put_u32(uint8_t *p, size_t off, uint32_t v) {
   p[off] = (uint8_t)(v >> 24);
   p[off + 1] = (uint8_t)(v >> 16);
   p[off + 2] = (uint8_t)(v >> 8);
   p[off + 3] = (uint8_t)(v & 0xff);
}

static void add_table(uint8_t *p, size_t index, const char tag[4], uint32_t off, uint32_t len) {
   size_t dir = 12 + 16 * index;
   memcpy(p + dir, tag, 4);
   put_u32(p, dir + 8, off);
   put_u32(p, dir + 12, len);
}

static size_t build_truncated_glyph_font(uint8_t *font, size_t cap) {
   size_t off;
   size_t cmap, head, hhea, hmtx, maxp, loca, glyf, g0;
   memset(font, 0, cap);

   put_u32(font, 0, 0x00010000);
   put_u16(font, 4, 7);
   put_u16(font, 6, 64);
   put_u16(font, 8, 2);
   put_u16(font, 10, 48);
   off = 12 + 16 * 7;

   cmap = off;
   add_table(font, 0, "cmap", (uint32_t)cmap, 268);
   put_u16(font, cmap, 0);
   put_u16(font, cmap + 2, 1);
   put_u16(font, cmap + 4, 3);
   put_u16(font, cmap + 6, 1);
   put_u32(font, cmap + 8, 12);
   put_u16(font, cmap + 12, 0);
   put_u16(font, cmap + 14, 262);
   off = cmap + 268;

   head = off;
   add_table(font, 1, "head", (uint32_t)head, 54);
   put_u16(font, head + 18, 1024);
   put_u16(font, head + 50, 1);
   off = head + 54;

   hhea = off;
   add_table(font, 2, "hhea", (uint32_t)hhea, 36);
   put_u16(font, hhea + 34, 1);
   off = hhea + 36;

   hmtx = off;
   add_table(font, 3, "hmtx", (uint32_t)hmtx, 4);
   off = hmtx + 4;

   maxp = off;
   add_table(font, 4, "maxp", (uint32_t)maxp, 6);
   put_u16(font, maxp + 4, 1);
   off = maxp + 6;

   loca = off;
   add_table(font, 5, "loca", (uint32_t)loca, 8);
   off = loca + 8;

   glyf = off;
   g0 = off;
   put_u16(font, off, 1);       /* one contour */
   put_u16(font, off + 2, 0);
   put_u16(font, off + 4, 0);
   put_u16(font, off + 6, 100);
   put_u16(font, off + 8, 100);
   off += 10;
   put_u16(font, off, 10000);   /* endPtsOfContours says 10001 points */
   off += 2;
   put_u16(font, off, 0);       /* no instructions */
   off += 2;
   font[off++] = 1;             /* only one flag byte */
   font[off++] = 1;             /* only one coordinate byte */

   put_u32(font, loca, (uint32_t)(g0 - glyf));
   put_u32(font, loca + 4, (uint32_t)(off - glyf));
   add_table(font, 6, "glyf", (uint32_t)glyf, (uint32_t)(off - glyf));
   return off;
}

int main(void) {
   uint8_t *scratch = (uint8_t *)calloc(1, 1024);
   uint8_t *font;
   size_t size;
   stbtt_fontinfo info;
   stbtt_vertex *vertices = NULL;
   int n;

   if (!scratch) return 2;
   size = build_truncated_glyph_font(scratch, 1024);
   font = (uint8_t *)malloc(size);
   if (!font) {
      free(scratch);
      return 2;
   }
   memcpy(font, scratch, size);
   free(scratch);

   if (!stbtt_InitFont(&info, font, 0)) {
      free(font);
      return 2;
   }

   n = stbtt_GetGlyphShape(&info, 0, &vertices);
   if (vertices)
      stbtt_FreeShape(&info, vertices);
   free(font);
   return n == 0 ? 0 : 1;
}
