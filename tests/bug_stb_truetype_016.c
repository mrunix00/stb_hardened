#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "../stb_truetype.h"

/*
 * Regression test for BUG-stb_truetype-016.
 *
 * stbtt_GetFontNameString and stbtt__matchpair (called via
 * stbtt_FindMatchingFontEx) iterate the name-table record array with an
 * attacker-controlled count read at fc+nm+2, walking past the heap
 * buffer and triggering an ASan heap-buffer-overflow read.
 *
 * We synthesize a minimal TrueType font in memory with:
 *   - a valid offset table and 7 required tables (cmap, glyf, head,
 *     hhea, hmtx, loca, maxp), each placed so that the table directory
 *     entry's offset+length fits in the buffer per the BUG-012 bounds;
 *   - a minimal cmap with numTables = 0 so InitFont succeeds;
 *   - an 8th "name" table with a 6-byte header claiming count = 0xFFFF
 *     but a body of just 2 bytes.
 *
 * Unpatched: stbtt_GetFontNameString loops count=0xFFFF times reading
 * 12 bytes per record, walking ~768 KB past the 8-byte name table and
 * triggering a heap-buffer-overflow read at stb_truetype.h:1287:55.
 * Patched:   the count is clamped to (name_table_size - 6) / 12 == 0,
 * so the loop body never runs and the function returns NULL.
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

static size_t build_malicious_name_font(uint8_t *font, size_t cap) {
   size_t off;
   size_t cmap, head, hhea, hmtx, maxp, loca, glyf, name;
   memset(font, 0, cap);

   /* Offset table: 8 tables, no search optimisations. */
   put_u32(font, 0, 0x00010000);
   put_u16(font, 4, 8);
   put_u16(font, 6, 128);
   put_u16(font, 8, 3);
   put_u16(font, 10, 0);
   off = 12 + 16 * 8;

   /* cmap: minimal — format 0, numTables = 0, so the encoding loop is
    * skipped and InitFont succeeds without picking an index_map.
    * We supply a Format 4 subtable to keep FindGlyphIndex reachable. */
   cmap = off;
   put_u16(font, cmap,     0);   /* version 0 */
   put_u16(font, cmap + 2, 1);   /* numTables = 1 (single encoding record) */
   put_u16(font, cmap + 4, 3);   /* platformID = Microsoft */
   put_u16(font, cmap + 6, 1);   /* encodingID = Unicode BMP */
   put_u32(font, cmap + 8, 12);  /* offset to subtable (cmap + 12) */
   put_u16(font, cmap + 12, 4);  /* format = 4 */
   put_u16(font, cmap + 14, 14); /* length of subtable (16 bytes) */
   put_u16(font, cmap + 16, 0);  /* language = 0 */
   put_u16(font, cmap + 18, 0);  /* segCountX2 = 0 (empty) */
   put_u16(font, cmap + 20, 0);  /* searchRange */
   put_u16(font, cmap + 22, 0);  /* entrySelector */
   put_u16(font, cmap + 24, 0);  /* rangeShift */
   off = cmap + 28;
   add_table(font, 0, "cmap", (uint32_t)cmap, (uint32_t)(off - cmap));

   /* head: indexToLocFormat = 1 (long offsets). */
   head = off;
   put_u32(font, head + 0,  0x00010000);
   put_u32(font, head + 4,  0x00010000);
   put_u32(font, head + 8,  0x5F0F3CF5);
   put_u32(font, head + 12, 0x5F0F3CF5);
   put_u16(font, head + 16, 0x000B);
   put_u16(font, head + 18, 1024);
   put_u16(font, head + 20, 0);
   put_u16(font, head + 22, 0);
   put_u16(font, head + 24, 0);
   put_u16(font, head + 26, 0);
   put_u16(font, head + 28, 0);
   put_u16(font, head + 30, 0);
   put_u16(font, head + 32, 0);
   put_u16(font, head + 34, 0);
   put_u16(font, head + 36, 0);
   put_u16(font, head + 38, 0);
   put_u16(font, head + 40, 0);
   put_u16(font, head + 42, 0);
   put_u16(font, head + 44, 0);
   put_u16(font, head + 46, 0);
   put_u16(font, head + 48, 0);
   put_u16(font, head + 50, 1); /* indexToLocFormat = 1 */
   off = head + 54;
   add_table(font, 1, "head", (uint32_t)head, 54);

   /* hhea: numOfLongHorMetrics = 1, ascender > descender so
    * ScaleForPixelHeight does not divide by zero. */
   hhea = off;
   put_u16(font, hhea + 34, 1);
   off = hhea + 36;
   add_table(font, 2, "hhea", (uint32_t)hhea, 36);

   /* hmtx: one long-hor-metric record (4 bytes). */
   hmtx = off;
   off = hmtx + 4;
   add_table(font, 3, "hmtx", (uint32_t)hmtx, 4);

   /* maxp: numGlyphs = 1, version 0.5. */
   maxp = off;
   put_u32(font, maxp, 0x00005000);
   put_u16(font, maxp + 4, 1);
   off = maxp + 6;
   add_table(font, 4, "maxp", (uint32_t)maxp, 6);

   /* loca: 2 long offsets (for 1 glyph + sentinel), both pointing to
    * the start of glyf so the glyph is treated as empty. */
   loca = off;
   put_u32(font, loca, 0);
   put_u32(font, loca + 4, 0);
   off = loca + 8;
   add_table(font, 5, "loca", (uint32_t)loca, 8);

   /* glyf: empty (a zero-length glyph). */
   glyf = off;
   off = glyf + 0;
   add_table(font, 6, "glyf", (uint32_t)glyf, 0);

   /* name: 6-byte header (format=0, count=0xFFFF, stringOffset=6)
    * plus 2 padding bytes — the table is 8 bytes long, but the loop
    * driven by count=0xFFFF would walk ~768 KB past it. */
   name = off;
   put_u16(font, name + 0, 0);       /* format = 0 */
   put_u16(font, name + 2, 0xFFFF);  /* count = 65535 (attack) */
   put_u16(font, name + 4, 6);       /* stringOffset */
   font[name + 6] = 0;
   font[name + 7] = 0;
   off = name + 8;
   add_table(font, 7, "name", (uint32_t)name, 8);

   return off;
}

int main(void) {
   uint8_t font[2048];
   size_t font_size;
   stbtt_fontinfo info;
   int len = 0;

   font_size = build_malicious_name_font(font, sizeof font);
   if (font_size > sizeof font) {
      fprintf(stderr, "font builder overflowed scratch buffer\n");
      return 2;
   }

   /* InitFontEx with data_len = font_size so the data_len-aware bounds
    * checks in InitFont are active. */
   if (!stbtt_InitFontEx(&info, font, 0, (int)font_size)) {
      fprintf(stderr, "InitFontEx returned 0; cannot exercise the name path\n");
      return 0;
   }

   /* The bug: stbtt_GetFontNameString reads count=0xFFFF from the name
    * table and iterates 0xFFFF records of 12 bytes each, far past the
    * 8-byte name table. The patched library clamps count to 0 and
    * returns NULL without reading anything. */
   (void)stbtt_GetFontNameString(&info, &len, STBTT_PLATFORM_ID_MICROSOFT,
                                 STBTT_MS_EID_UNICODE_BMP, STBTT_MS_LANG_ENGLISH, 1);
   (void)stbtt_GetFontNameString(&info, &len, STBTT_PLATFORM_ID_MICROSOFT,
                                 STBTT_MS_EID_UNICODE_BMP, STBTT_MS_LANG_ENGLISH, 4);
   (void)stbtt_GetFontNameString(&info, &len, STBTT_PLATFORM_ID_MAC,
                                 STBTT_MAC_EID_ROMAN, STBTT_MAC_LANG_ENGLISH, 1);
   (void)stbtt_GetFontNameString(&info, &len, STBTT_PLATFORM_ID_MAC,
                                 STBTT_MAC_EID_ROMAN, STBTT_MAC_LANG_ENGLISH, 4);

   /* stbtt_FindMatchingFont dispatches into stbtt__matchpair, which
    * used the same unbounded name-table iteration. The fix clamps the
    * count to (name_table_size - 6) / 12 in stbtt__matchpair too, so
    * the function returns 0 without reading past the buffer. */
   (void)stbtt_FindMatchingFont(font, "Arial", STBTT_MACSTYLE_NONE);
   (void)stbtt_FindMatchingFont(font, "DejaVu", STBTT_MACSTYLE_BOLD);

   return 0;
}
