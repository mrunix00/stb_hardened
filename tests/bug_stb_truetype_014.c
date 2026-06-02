#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "../stb_truetype.h"

/*
 * Regression test for BUG-stb_truetype-014.
 *
 * stbtt__find_table bounds the table-directory iteration against
 * data_len (BUG-012) but once it finds a matching tag it returns the
 * raw font-controlled offset without verifying that the offset lies
 * within the buffer. Every downstream ttUSHORT/ttULONG read on the
 * returned offset can then OOB.
 *
 * We synthesize a minimal TrueType font with 7 required tables. Six
 * of them (cmap, head, hhea, hmtx, loca, glyf) have valid offsets and
 * pass the early InitFont checks. The 7th (maxp) is deliberately
 * placed at offset = buffer_size, so the very next dereference
 * `ttUSHORT(data + maxp + 4)` in stbtt_InitFont_internal:1500 reads
 * 4 bytes past the end of the heap allocation.
 *
 * Unpatched: ASan aborts the process at stb_truetype.h:1287:55 in
 * ttUSHORT called from stb_truetype.h:1500 in stbtt_InitFont_internal.
 * Patched:   stbtt__find_table returns 0 for the maxp entry because
 * the offset is >= data_len; numGlyphs falls back to 0xffff (line
 * 1502) and InitFontEx eventually returns 0 because no other required
 * table has a bogus offset to exploit. The test exits 0.
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
   enum { font_size = 256 };
   uint8_t *font = (uint8_t *)calloc(1, font_size);
   if (!font) return 2;
   stbtt_fontinfo info;

   /* Offset table: 7 tables. */
   put_u32(font, 0, 0x00010000);
   put_u16(font, 4, 7);
   put_u16(font, 6, 64);
   put_u16(font, 8, 2);
   put_u16(font, 10, 64);

   /* Directory entries. The 7th (maxp) has a deliberately invalid
    * offset equal to font_size; the high byte is zero so the ttULONG
    * read at find_table:1328 does not also trip BUG-019's signed
    * shift UB. */
   add_table(font, 0, "cmap",  200, 24);
   add_table(font, 1, "head",  120, 54);
   add_table(font, 2, "hhea",  174, 36);
   add_table(font, 3, "hmtx",  210, 4);
   add_table(font, 4, "loca",  214, 8);
   add_table(font, 5, "glyf",  222, 0);
   add_table(font, 6, "maxp", (uint32_t)font_size, 6);

   /* The data length is exactly the heap allocation size so the patched
    * find_table can correctly reject the maxp entry. */
   int rc = stbtt_InitFontEx(&info, font, 0, font_size);
   free(font);
   (void)rc;  /* the bug was the OOB; the return value is not part of the test. */
   return 0;
}
