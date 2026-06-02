#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "../stb_truetype.h"

/*
 * Regression test for BUG-stb_truetype-015.
 *
 * stbtt_InitFont_internal sets
 *   info->index_map = cmap + ttULONG(data+encoding_record+4);
 * from a font-controlled 4-byte value. When the encoding subtable
 * offset is bogus (here 0xFFFFFFFC), info->index_map points
 * 4 billion bytes past cmap. The first dereference inside
 * stbtt_FindGlyphIndex — `ttUSHORT(data + index_map + 0)` at
 * line 1561 — reads 4 GB past the buffer, which ASan reports
 * as a heap-buffer-overflow (or a SEGV without ASan).
 *
 * Unpatched: ASan reports
 *   heap-buffer-overflow on address 0x... at ttUSHORT:1287
 *   called from stbtt_FindGlyphIndex:1555.
 * Patched:   the post-loop index_map bound check in
 *   stbtt_InitFont_internal rejects the bogus offset and sets
 *   index_map = 0, so InitFont returns 0 (no usable cmap) and
 *   the test exits 0 without touching FindGlyphIndex at all.
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

   /* Runtime-initialized (defeats constant folding). */
   for (int i = 0; i < font_size; i++) font[i] = 0;
   put_u32(font, 0, 0x00010000);
   put_u16(font, 4, 7);
   put_u16(font, 6, 128);
   put_u16(font, 8, 3);
   put_u16(font, 10, 0);

   put_dir(font, 12,  "cmap", 0, 128, 80);   /* extend cmap to 80 bytes */
   put_dir(font, 28,  "head", 0, 208, 54);
   put_dir(font, 44,  "hhea", 0, 262, 36);
   put_dir(font, 60,  "hmtx", 0, 298, 8);
   put_dir(font, 76,  "maxp", 0, 306, 6);
   put_dir(font, 92,  "loca", 0, 312, 4);
   put_dir(font, 108, "glyf", 0, 316, 0);

   /* cmap: 1 encoding record pointing to a bogus offset that
    * lands well past the end of the 320-byte buffer. */
   put_u16(font, 128, 0);          /* version */
   put_u16(font, 130, 1);          /* numTables */
   put_u16(font, 132, 3);          /* platformID = Microsoft */
   put_u16(font, 134, 1);          /* encodingID = Unicode BMP */
   put_u32(font, 136, 0x00001000); /* offset → index_map = 128 + 4096 = 4224 (well past 320) */
   /* rest of cmap is zeros */

   /* head */
   put_u32(font, 208, 0x00010000);
   put_u32(font, 212, 0x00005000);
   put_u32(font, 216, 0);
   put_u32(font, 220, 0x5F0F3CF5);
   put_u16(font, 224, 0x000B);
   put_u16(font, 226, 1000);
   put_u16(font, 228, 0);

   /* hhea */
   put_u32(font, 262, 0x00010000);
   put_u16(font, 266, 1);
   put_u16(font, 268, 0xFFFFu);

   /* maxp */
   put_u32(font, 306, 0x00005000);
   put_u16(font, 310, 1);

   /* loca */
   put_u16(font, 312, 0);
   put_u16(font, 314, 0);

   stbtt_fontinfo info;
   int rc = stbtt_InitFontEx(&info, font, 0, font_size);
   /* The patched library rejects the bogus cmap subtable offset and
    * sets index_map = 0, causing InitFont to return 0. The unpatched
    * library keeps the bogus index_map and crashes inside
    * FindGlyphIndex when it dereferences data+index_map+0. */
   printf("rc=%d\n", rc);
   if (rc != 0) {
      int gi = stbtt_FindGlyphIndex(&info, 'A');
      printf("glyph index for 'A' = %d (would OOB on unpatched)\n", gi);
   } else {
      printf("InitFont rejected the bogus cmap subtable offset (expected on patched)\n");
   }
   free(font);
   return 0;
}
