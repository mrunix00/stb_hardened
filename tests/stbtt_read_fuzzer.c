#ifdef __cplusplus
extern "C" {
#endif

#define STB_TRUETYPE_IMPLEMENTATION

#include "stb_truetype.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Comprehensive fuzzer for stb_truetype.h.
 *
 * Layout: first byte selects which API to exercise; remaining bytes
 * are the font payload. We deliberately use stbtt_InitFontEx with the
 * actual buffer length so the fuzzer exercises the bounds-checked init
 * path (BUG-012). Most APIs that operate on a parsed font come after a
 * successful Init so we don't waste cycles on garbage.
 */

static const float kPixelHeights[4] = { 16.0f, 32.0f, 64.0f, 128.0f };
static const int   kCodepointPool[8] = { 'A', 'g', '0', 0x20AC, 0x4E2D, 0xFFFD, 0x10437, 0x1F600 };

#ifdef STBTT_FUZZ_SAFE_INIT
/*
 * Fuzzer-only precondition: reject inputs where the table directory
 * advertises offsets/lengths that overflow the buffer. Without this,
 * the fuzzer keeps tripping the find_table OOB read (BUG-014) before
 * it can reach deeper code paths.
 */
static uint32_t safe_get_u32(const uint8_t *p, int psize, int off) {
   if (off < 0 || off + 4 > psize) return 0xffffffffu;
   return ((uint32_t)p[off] << 24) | ((uint32_t)p[off+1] << 16) |
          ((uint32_t)p[off+2] << 8) | (uint32_t)p[off+3];
}
static uint16_t safe_get_u16(const uint8_t *p, int psize, int off) {
   if (off < 0 || off + 2 > psize) return 0xffffu;
   return ((uint16_t)p[off] << 8) | (uint16_t)p[off+1];
}
static int input_passes_directory_check(const uint8_t *payload, int psize, int font_offset)
{
   if (font_offset < 0 || font_offset + 12 > psize) return 0;
   uint16_t num = safe_get_u16(payload, psize, font_offset + 4);
   if (num == 0 || num > 64) return 0;
   uint32_t dir_end = (uint32_t)font_offset + 12u + (uint32_t)num * 16u;
   if (dir_end > (uint32_t)psize) return 0;
   for (int i = 0; i < num; ++i) {
      int loc = font_offset + 12 + 16 * i;
      uint32_t off = safe_get_u32(payload, psize, loc + 8);
      uint32_t len = safe_get_u32(payload, psize, loc + 12);
      uint64_t end = (uint64_t)off + (uint64_t)len;
      if (off > (uint32_t)psize || end > (uint32_t)psize) return 0;
   }
   return 1;
}
#endif

static void exercise_font_directory(const uint8_t *data, int size)
{
   int num = stbtt_GetNumberOfFonts(data);
   if (num > 0) {
      int i;
      for (i = 0; i < num && i < 8; ++i)
         (void)stbtt_GetFontOffsetForIndex(data, i);
   }
#ifndef STBTT_FUZZ_SKIP_FINDMATCH
   (void)stbtt_FindMatchingFont(data, "Arial", STBTT_MACSTYLE_NONE);
   (void)stbtt_FindMatchingFont(data, "DejaVu", STBTT_MACSTYLE_BOLD);
#endif
   (void)size;
}

static void exercise_metrics_and_names(const stbtt_fontinfo *info)
{
   int ascent = 0, descent = 0, lineGap = 0;
   int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
   int tA = 0, tD = 0, tL = 0;
   int len = 0;

   stbtt_GetFontVMetrics(info, &ascent, &descent, &lineGap);
   (void)stbtt_GetFontVMetricsOS2(info, &tA, &tD, &tL);
   stbtt_GetFontBoundingBox(info, &x0, &y0, &x1, &y1);
#ifndef STBTT_FUZZ_SKIP_NAMESTRING
   (void)stbtt_GetFontNameString(info, &len, STBTT_PLATFORM_ID_MAC, STBTT_MAC_EID_ROMAN,
                                 STBTT_MAC_LANG_ENGLISH, 1);
   (void)stbtt_GetFontNameString(info, &len, STBTT_PLATFORM_ID_MICROSOFT,
                                 STBTT_MS_EID_UNICODE_BMP, STBTT_MS_LANG_ENGLISH, 4);
#else
   (void)len;
#endif
}

static void exercise_glyph_shape(const stbtt_fontinfo *info, int glyph)
{
   stbtt_vertex *vertices = NULL;
   int n = stbtt_GetGlyphShape(info, glyph, &vertices);
   if (vertices && n > 0) {
      stbtt_FreeShape(info, vertices);
   } else if (vertices) {
      stbtt_FreeShape(info, vertices);
   }
}

static void exercise_glyph_bitmap(const stbtt_fontinfo *info, int glyph, float pixel_height)
{
   int w = 0, h = 0, xoff = 0, yoff = 0;
   unsigned char *bitmap;
   float scale = stbtt_ScaleForPixelHeight(info, pixel_height);
   if (!(scale > 0.0f) || !(scale < 16.0f))
      return;
   bitmap = stbtt_GetGlyphBitmap(info, scale, scale, glyph, &w, &h, &xoff, &yoff);
   if (bitmap)
      stbtt_FreeBitmap(bitmap, NULL);
}

static void exercise_glyph_sdf(const stbtt_fontinfo *info, int glyph)
{
   int w = 0, h = 0, xoff = 0, yoff = 0;
   unsigned char *sdf;
#ifndef STBTT_FUZZ_SKIP_SDF
   sdf = stbtt_GetGlyphSDF(info, 1.0f / 64.0f, glyph, 2, 128, 32.0f,
                           &w, &h, &xoff, &yoff);
   if (sdf)
      stbtt_FreeSDF(sdf, NULL);
#else
   (void)info; (void)glyph; (void)w; (void)h; (void)xoff; (void)yoff; (void)sdf;
#endif
}

static void exercise_kerning(const stbtt_fontinfo *info)
{
   int n = stbtt_GetKerningTableLength(info);
   if (n > 0 && n < 4096) {
      stbtt_kerningentry *table = (stbtt_kerningentry *)calloc((size_t)n, sizeof(*table));
      if (table) {
         (void)stbtt_GetKerningTable(info, table, n);
         free(table);
      }
   }
   (void)stbtt_GetCodepointKernAdvance(info, 'A', 'V');
   (void)stbtt_GetGlyphKernAdvance(info, 1, 2);
}

static void exercise_pack(const uint8_t *data, int size, int font_offset)
{
   int pw = 64, ph = 64;
   unsigned char *atlas;
   stbtt_pack_context spc;
   stbtt_packedchar chardata[16];
#ifdef STBTT_FUZZ_SKIP_PACK
   (void)data; (void)size; (void)font_offset; (void)pw; (void)ph; (void)atlas; (void)spc; (void)chardata;
   return;
#else
   atlas = (unsigned char *)calloc((size_t)(pw * ph), 1);
   if (!atlas) return;

   if (stbtt_PackBegin(&spc, atlas, pw, ph, 0, 1, NULL)) {
      stbtt_PackSetOversampling(&spc, 1, 1);
      (void)stbtt_PackFontRange(&spc, data, font_offset, 16.0f, ' ', 16, chardata);
      stbtt_PackEnd(&spc);
   }
   free(atlas);
   (void)size;
#endif
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
   if (size < 8 || size > (1u << 22)) return 0;

   uint8_t mode = data[0];
   const uint8_t *payload = data + 1;
   int psize = (int)(size - 1);

   /* Always exercise the directory-walking helpers — they parse the offset
    * table independently of Init. */
   exercise_font_directory(payload, psize);

   /* Pick a font index from the input. */
   int font_index = (data[1] >> 4) & 0x0f;
   int font_offset = stbtt_GetFontOffsetForIndex(payload, font_index);
   if (font_offset < 0)
      font_offset = 0;

#ifdef STBTT_FUZZ_SAFE_INIT
   if (!input_passes_directory_check(payload, psize, font_offset))
      return 0;
#endif

   stbtt_fontinfo info;
   if (!stbtt_InitFontEx(&info, payload, font_offset, psize))
      return 0;

#ifdef STBTT_FUZZ_SKIP_INIT_POST
   return 0;
#endif

   /* Quick metrics + name reads. */
   exercise_metrics_and_names(&info);

   /* Glyph lookups for a small spread of codepoints, plus a raw glyph ID. */
   int codepoint = kCodepointPool[mode % 8];
#ifndef STBTT_FUZZ_SKIP_FINDGLYPH
   int glyph = stbtt_FindGlyphIndex(&info, codepoint);
#else
   int glyph = 0;
#endif
   int raw_glyph = (int)((mode >> 3) & 0x1f); /* small numeric glyph id */
   float ph = kPixelHeights[(mode >> 1) & 3];

   switch (mode & 0x7) {
   case 0:
      exercise_glyph_shape(&info, glyph);
      break;
   case 1:
      exercise_glyph_shape(&info, raw_glyph);
      break;
   case 2:
      exercise_glyph_bitmap(&info, glyph, ph);
      break;
   case 3:
      exercise_glyph_bitmap(&info, raw_glyph, ph);
      break;
   case 4:
      exercise_glyph_sdf(&info, glyph);
      break;
   case 5:
      exercise_kerning(&info);
      break;
   case 6:
      exercise_pack(payload, psize, font_offset);
      break;
   case 7: {
      /* Box / metric helpers on raw glyph ids; these reach loca/glyf
       * and head/hhea parsing paths that fonts can lie about. */
      int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
      int adv = 0, lsb = 0;
      (void)stbtt_GetGlyphBox(&info, raw_glyph, &x0, &y0, &x1, &y1);
      stbtt_GetGlyphHMetrics(&info, raw_glyph, &adv, &lsb);
      (void)stbtt_IsGlyphEmpty(&info, raw_glyph);
      const char *svg = NULL;
      (void)stbtt_GetGlyphSVG(&info, raw_glyph, &svg);
      break;
   }
   }

   return 0;
}

#ifdef __cplusplus
}
#endif
