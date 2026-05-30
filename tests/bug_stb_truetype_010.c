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
   // Build a minimal TrueType font (256 bytes). The cmap table has only a
   // 4-byte header (version + numTables) with numTables set to 100, but no
   // encoding records follow. The encoding selection loop at stb_truetype.h:1488
   // reads the first encoding record at data + cmap + 4, which is past the end
   // of the buffer.
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
   //   252-255  cmap  (4 bytes — header only)
   //   ^^^^^^^^ buffer ends here; encoding record read at +256 is OOB

   size_t font_size = 256;
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
   static uint32_t  tlen[7] = {  4,  54,  36,   4,   6,   8,  20};
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

   // --- cmap header (252) — only 4 bytes, no encoding records --------------
   put_u16(font, 252, 0);           // version
   put_u16(font, 254, 100);         // numTables = 100 -> loop reads OOB at +256

   stbtt_fontinfo info;
   int init_ok = stbtt_InitFont(&info, font, 0);
   printf("InitFont: %d\n", init_ok);
   free(font);
   return 0;
}
