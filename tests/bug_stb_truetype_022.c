#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "../stb_truetype.h"

/*
 * Regression test for BUG-stb_truetype-022.
 *
 * stbtt__GetGlyfOffset reads the loca table to translate a glyph
 * index into a byte offset within the glyf table, but trusts the
 * font-controlled loca entry value to compute the final offset
 * without verifying that the resulting offset lies within the font
 * buffer. Callers like stbtt_GetGlyphBox:1707 then dereference
 * ttSHORT(info->data + g + 2) at the bogus offset, triggering an
 * ASan OOB read or SEGV.
 *
 * We synthesize a minimal TrueType font with a malformed loca table:
 * one entry claiming glyph 0 starts at offset 0x7FFE (which, when
 * added to info->glyf and multiplied, points well past the 256-byte
 * buffer). numGlyphs is 1 so the glyph_index bounds check passes.
 *
 * Unpatched: ASan reports an OOB or SEGV at stb_truetype.h:1288:55
 * in ttSHORT called from stbtt_GetGlyphBox:1707.
 * Patched:   stbtt__GetGlyfOffset validates that the loca-derived
 * offset is within the font buffer and returns -1; GetGlyphBox then
 * returns 0 without dereferencing anything.
 */

static void put_u16(uint8_t *p, size_t off, uint16_t v) {
   p[off]     = (uint8_t)(v >> 8);
   p[off + 1] = (uint8_t)(v & 0xff);
}

static void put_u32(uint8_t *p, size_t off, uint32_t v) {
   p[off]     = (uint8_t)(v >> 24);
   p[off + 1] = (uint8_t)(v >> 16);
   p[off + 2] = (uint8_t)(v >> 8);
   p[off + 3] = (uint8_t)(v & 0xff);
}

static void add_table(uint8_t *p, size_t index, const char tag[4], uint32_t off, uint32_t len) {
   size_t dir = 12 + 16 * index;
   memcpy(p + dir, tag, 4);
   put_u32(p, dir + 8,  off);
   put_u32(p, dir + 12, len);
}

int main(void) {
   enum { font_size = 320 };
   uint8_t *font = (uint8_t *)calloc(1, font_size);
   if (!font) return 2;
   stbtt_fontinfo info;

   /* Offset table: 7 tables. */
   put_u32(font, 0, 0x00010000);
   put_u16(font, 4, 7);
   put_u16(font, 6, 64);
   put_u16(font, 8, 2);
   put_u16(font, 10, 64);

   /* Place the tables contiguously starting at byte 124 (12 header
    * + 7*16 directory). indexToLocFormat = 0 (short loca entries).
    * cmap supplies a single Format 4 subtable (1 segment) so InitFont
    * sets index_map and proceeds to the glyf path. */
   const size_t cmap_off  = 124, cmap_len  = 32;
   const size_t head_off  = cmap_off + cmap_len, head_len = 54;
   const size_t hhea_off  = head_off + head_len, hhea_len = 36;
   const size_t hmtx_off  = hhea_off + hhea_len, hmtx_len = 4;
   const size_t loca_off  = hmtx_off + hmtx_len, loca_len = 4;
   const size_t glyf_off  = loca_off + loca_len, glyf_len = 0;
   const size_t maxp_off  = glyf_off + glyf_len, maxp_len = 6;

   /* cmap header (12 bytes) + a single MS/Unicode encoding record
    * pointing at a Format 4 subtable (20 bytes). Total 32 bytes. */
   put_u16(font, cmap_off + 0, 0);          /* version 0 */
   put_u16(font, cmap_off + 2, 1);          /* numTables = 1 */
   put_u16(font, cmap_off + 4, 3);          /* platformID = MS */
   put_u16(font, cmap_off + 6, 1);          /* encodingID = Unicode BMP */
   put_u32(font, cmap_off + 8, 12);         /* offset to subtable (cmap+12) */
   put_u16(font, cmap_off + 12, 4);         /* format = 4 */
   put_u16(font, cmap_off + 14, 20);        /* length of subtable */
   put_u16(font, cmap_off + 16, 0);         /* language = 0 */
   put_u16(font, cmap_off + 18, 2);         /* segCountX2 = 2 (1 segment) */
   put_u16(font, cmap_off + 20, 2);         /* searchRange */
   put_u16(font, cmap_off + 22, 0);         /* entrySelector */
   put_u16(font, cmap_off + 24, 0);         /* rangeShift */
   put_u16(font, cmap_off + 26, 0xFFFF);    /* endCount[0] = sentinel */
   put_u16(font, cmap_off + 28, 0);         /* reservedPad */
   put_u16(font, cmap_off + 30, 0xFFFF);    /* startCount[0] = sentinel */

   /* head: indexToLocFormat = 0. */
   put_u32(font, head_off, 0x00010000);
   put_u16(font, head_off + 50, 0);

   /* hhea: numOfLongHorMetrics = 1. */
   put_u16(font, hhea_off + 34, 1);

   /* loca: format 0 (2-byte entries). Entry 0 is the malicious value
    * 0x7FFE; the *2 multiplier in stbtt__GetGlyfOffset:1684 turns
    * this into a 0xFFFC-byte offset past info->glyf, which is well
    * beyond the buffer. Entry 1 is the sentinel = 0. */
   put_u16(font, loca_off + 0, 0x7FFE);
   put_u16(font, loca_off + 2, 0x0000);

   /* maxp: version 0.5, numGlyphs = 1. */
   put_u32(font, maxp_off, 0x00005000);
   put_u16(font, maxp_off + 4, 1);

   add_table(font, 0, "cmap", (uint32_t)cmap_off, (uint32_t)cmap_len);
   add_table(font, 1, "head", (uint32_t)head_off, (uint32_t)head_len);
   add_table(font, 2, "hhea", (uint32_t)hhea_off, (uint32_t)hhea_len);
   add_table(font, 3, "hmtx", (uint32_t)hmtx_off, (uint32_t)hmtx_len);
   add_table(font, 4, "loca", (uint32_t)loca_off, (uint32_t)loca_len);
   add_table(font, 5, "glyf", (uint32_t)glyf_off, (uint32_t)glyf_len);
   add_table(font, 6, "maxp", (uint32_t)maxp_off, (uint32_t)maxp_len);

   if (!stbtt_InitFontEx(&info, font, 0, font_size)) {
      fprintf(stderr, "InitFontEx returned 0; cannot exercise the loca path\n");
      free(font);
      return 0;
   }

   /* The bug: stbtt_GetGlyphBox reads ttSHORT(info->data + g + 2) for
    * glyph 0 where g = info->glyf + 0x7FFE * 2 = 0xFFFC. The patched
    * library returns -1 from stbtt__GetGlyfOffset (g1 is out of
    * range), GetGlyphBox returns 0, and no OOB occurs. */
   int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
   (void)stbtt_GetGlyphBox(&info, 0, &x0, &y0, &x1, &y1);

   free(font);
   return 0;
}
