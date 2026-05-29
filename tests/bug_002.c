#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "../stb_truetype.h"

#define FONT_SIZE (4 << 20)

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

static void add_table(uint8_t *p, size_t index, const char tag[4],
                       uint32_t off, uint32_t len) {
  size_t dir = 12 + 16 * index;
  memcpy(p + dir, tag, 4);
  put_u32(p, dir + 8, off);
  put_u32(p, dir + 12, len);
}

static size_t build_font(uint8_t *font, int pts, int ncomp) {
  memset(font, 0, FONT_SIZE);
  put_u32(font, 0, 0x00010000);
  put_u16(font, 4, 7);
  put_u16(font, 6, 64);
  put_u16(font, 8, 2);
  put_u16(font, 10, 48);

  size_t off = 12 + 16 * 7;

  size_t cmap_off = off, cmap_len = 268;
  add_table(font, 0, "cmap", (uint32_t)cmap_off, (uint32_t)cmap_len);
  put_u16(font, cmap_off, 0);
  put_u16(font, cmap_off + 2, 1);
  put_u16(font, cmap_off + 4, 0);
  put_u16(font, cmap_off + 6, 0);
  put_u32(font, cmap_off + 8, 12);
  put_u16(font, cmap_off + 12, 0);
  put_u16(font, cmap_off + 14, 262);
  off = cmap_off + cmap_len;

  size_t head_off = off, head_len = 54;
  add_table(font, 1, "head", (uint32_t)head_off, (uint32_t)head_len);
  put_u16(font, head_off + 18, 1024);
  put_u16(font, head_off + 50, 1);
  off = head_off + head_len;

  size_t hhea_off = off, hhea_len = 36;
  add_table(font, 2, "hhea", (uint32_t)hhea_off, (uint32_t)hhea_len);
  put_u16(font, hhea_off + 34, 1);
  off = hhea_off + hhea_len;

  size_t hmtx_off = off, hmtx_len = 4 * 2;
  add_table(font, 3, "hmtx", (uint32_t)hmtx_off, (uint32_t)hmtx_len);
  off = hmtx_off + hmtx_len;

  size_t maxp_off = off, maxp_len = 6;
  add_table(font, 4, "maxp", (uint32_t)maxp_off, (uint32_t)maxp_len);
  put_u16(font, maxp_off + 4, 2);
  off = maxp_off + maxp_len;

  size_t loca_off = off, loca_len = 4 * 3;
  add_table(font, 5, "loca", (uint32_t)loca_off, (uint32_t)loca_len);
  off = loca_off + loca_len;

  size_t glyf_off = off;

  size_t g0 = off;
  put_u16(font, off, 1);
  put_u16(font, off + 2, 0);
  put_u16(font, off + 4, 0);
  put_u16(font, off + 6, 2000);
  put_u16(font, off + 8, 2000);
  off += 10;
  put_u16(font, off, (uint16_t)(pts - 1));
  off += 2;
  put_u16(font, off, 0);
  off += 2;

  {
    int remaining = pts;
    while (remaining > 0) {
      int batch = remaining > 255 ? 255 : remaining;
      font[off++] = 0x3F;
      font[off++] = (uint8_t)(batch - 1);
      remaining -= batch;
    }
    for (int i = 0; i < pts; ++i)
      font[off++] = 1;
    for (int i = 0; i < pts; ++i)
      font[off++] = 1;
  }

  size_t g1 = off;
  put_u16(font, off, (uint16_t)-1);
  put_u16(font, off + 2, 0);
  put_u16(font, off + 4, 0);
  put_u16(font, off + 6, 2000);
  put_u16(font, off + 8, 2000);
  off += 10;

  for (int i = 0; i < ncomp; ++i) {
    uint16_t flags = 0x0001 | 0x0002 | (1<<3);
    if (i < ncomp - 1) flags |= (1<<5);
    put_u16(font, off, flags);
    off += 2;
    put_u16(font, off, 0);
    off += 2;
    put_u16(font, off, 0);
    off += 2;
    put_u16(font, off, 0);
    off += 2;
    put_u16(font, off, 16384);
    off += 2;
  }

  put_u32(font, loca_off, (uint32_t)(g0 - glyf_off));
  put_u32(font, loca_off + 4, (uint32_t)(g1 - glyf_off));
  put_u32(font, loca_off + 8, (uint32_t)(off - glyf_off));
  add_table(font, 6, "glyf", (uint32_t)glyf_off, (uint32_t)(off - glyf_off));
  return off;
}

int main(int argc, char **argv) {
  int pts   = 65535;
  int ncomp = 500;
  (void)argc; (void)argv;

  uint8_t *font = (uint8_t *)malloc(FONT_SIZE);
  if (!font) return 1;

  build_font(font, pts, ncomp);

  stbtt_fontinfo info;
  if (!stbtt_InitFont(&info, font, 0)) {
    fprintf(stderr, "stbtt_InitFont FAILED\n");
    free(font);
    return 2;
  }

  stbtt_vertex *v0 = NULL;
  int n0 = stbtt_GetGlyphShape(&info, 0, &v0);
  if (n0 <= 0) {
    fprintf(stderr, "Glyph 0 returned %d (expected > 0)\n", n0);
    if (v0) stbtt_FreeShape(&info, v0);
    free(font);
    return 3;
  }
  if (v0) stbtt_FreeShape(&info, v0);

  stbtt_vertex *v1 = NULL;
  int n1 = stbtt_GetGlyphShape(&info, 1, &v1);
  if (n1 <= 0) {
    fprintf(stderr, "Glyph 1 returned %d (expected > 0)\n", n1);
    if (v1) stbtt_FreeShape(&info, v1);
    free(font);
    return 4;
  }

  fprintf(stderr, "Glyph 0: %d verts   Glyph 1 (%d comps): %d verts\n",
          n0, ncomp, n1);

  if (v1) stbtt_FreeShape(&info, v1);
  free(font);
  return 0;
}
