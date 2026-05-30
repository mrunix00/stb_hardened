#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "../stb_truetype.h"

int main(void) {
   // Create a 5-byte buffer where the first 4 bytes match a valid TrueType
   // signature but the buffer is too small for a full offset table.
   // stbtt_InitFontEx with data_len=5 triggers the bounds check in
   // stbtt__find_table, returning 0 before reading the OOB byte.
   size_t font_size = 5;
   uint8_t *font = (uint8_t *)calloc(font_size, 1);
   if (!font) return 2;

   font[0] = 0x00; font[1] = 0x01; // OpenType 1.0 signature
   font[2] = 0x00; font[3] = 0x00;
   font[4] = 0x00; // num_tables high byte (low byte would be OOB)

   stbtt_fontinfo info;
   int init_ok = stbtt_InitFontEx(&info, font, 0, 5);
   printf("InitFont: %d\n", init_ok);
   free(font);
   return 0;
}
