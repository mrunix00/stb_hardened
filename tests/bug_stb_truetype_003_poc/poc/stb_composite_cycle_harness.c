#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

static void put_u16(uint8_t *p, size_t off, uint16_t v) {
  p[off] = (uint8_t)(v >> 8);
  p[off + 1] = (uint8_t)v;
}

static void put_u32(uint8_t *p, size_t off, uint32_t v) {
  p[off] = (uint8_t)(v >> 24);
  p[off + 1] = (uint8_t)(v >> 16);
  p[off + 2] = (uint8_t)(v >> 8);
  p[off + 3] = (uint8_t)v;
}

static void add_table(uint8_t *p, size_t index, const char tag[4], uint32_t off, uint32_t len) {
  size_t dir = 12 + 16 * index;
  memcpy(p + dir, tag, 4);
  put_u32(p, dir + 8, off);
  put_u32(p, dir + 12, len);
}

static size_t build_font(uint8_t *font, size_t cap, int cycle) {
  const size_t cmap = 0x080;
  const size_t head = 0x1a0;
  const size_t hhea = 0x1e0;
  const size_t hmtx = 0x220;
  const size_t maxp = 0x240;
  const size_t loca = 0x260;
  const size_t glyf = 0x280;
  const uint16_t glyph_len = cycle ? 16 : 10;

  memset(font, 0, cap);

  put_u32(font, 0, 0x00010000);
  put_u16(font, 4, 7);
  put_u16(font, 6, 64);
  put_u16(font, 8, 2);
  put_u16(font, 10, 48);
  add_table(font, 0, "cmap", (uint32_t)cmap, 268);
  add_table(font, 1, "glyf", (uint32_t)glyf, glyph_len);
  add_table(font, 2, "head", (uint32_t)head, 54);
  add_table(font, 3, "hhea", (uint32_t)hhea, 36);
  add_table(font, 4, "hmtx", (uint32_t)hmtx, 4);
  add_table(font, 5, "loca", (uint32_t)loca, 8);
  add_table(font, 6, "maxp", (uint32_t)maxp, 6);

  put_u16(font, cmap + 0, 0);
  put_u16(font, cmap + 2, 1);
  put_u16(font, cmap + 4, 0);
  put_u16(font, cmap + 6, 0);
  put_u32(font, cmap + 8, 12);
  put_u16(font, cmap + 12, 0);
  put_u16(font, cmap + 14, 262);

  put_u16(font, head + 50, 1);
  put_u16(font, hhea + 34, 1);
  put_u16(font, maxp + 4, 1);
  put_u32(font, loca + 0, 0);
  put_u32(font, loca + 4, glyph_len);

  put_u16(font, glyf + 0, cycle ? 0xffff : 0);
  if (cycle) {
    put_u16(font, glyf + 10, 0x0002);
    put_u16(font, glyf + 12, 0);
    font[glyf + 14] = 0;
    font[glyf + 15] = 0;
  }

  return glyf + glyph_len;
}

int main(int argc, char **argv) {
  uint8_t font[1024];
  stbtt_fontinfo info;
  stbtt_vertex *vertices = NULL;
  int cycle = argc > 1 && strcmp(argv[1], "cycle") == 0;
  int n;

  (void)build_font(font, sizeof(font), cycle);
  if (!stbtt_InitFont(&info, font, 0)) {
    fprintf(stderr, "stbtt_InitFont failed\n");
    return 2;
  }

  n = stbtt_GetGlyphShape(&info, 0, &vertices);
  printf("mode=%s vertices=%d\n", cycle ? "cycle" : "empty", n);
  if (vertices) {
    stbtt_FreeShape(&info, vertices);
  }
  return 0;
}
