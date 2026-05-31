// BUG-stb_image-014: PNG palette out-of-bounds read / uninitialized stack disclosure
// In stbi__expand_png_palette, the palette length is ignored (STBI_NOTUSED(len)),
// allowing pixel indices beyond pal_len to read uninitialized stack memory.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#include "stb_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// PNG: 1x1, 8-bit indexed (color type 3), 1 palette entry (R=0, G=255, B=0),
// pixel value = 255 (should be out of range). With the bug, pixel reads
// uninitialized stack data instead of being clamped to palette[0].
unsigned char test_png[] = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
  0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x03, 0x00, 0x00, 0x00, 0x28, 0xcb, 0x34,
  0xbb, 0x00, 0x00, 0x00, 0x03, 0x50, 0x4c, 0x54, 0x45, 0x00, 0xff, 0x00, 0x34, 0x5e, 0xc0, 0xa8,
  0x00, 0x00, 0x00, 0x0d, 0x49, 0x44, 0x41, 0x54, 0x78, 0x01, 0x01, 0x02, 0x00, 0xfd, 0xff, 0x00,
  0xff, 0x01, 0x01, 0x01, 0x00, 0x60, 0x87, 0x12, 0xb5, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e,
  0x44, 0xae, 0x42, 0x60, 0x82,
};

int main(void)
{
   int x, y, comp;
   unsigned char *img = stbi_load_from_memory(test_png, sizeof(test_png), &x, &y, &comp, 0);
   if (!img) {
      // If the fix chose to return an error for out-of-range indices, accept that
      printf("PASS: Corrupt palette rejected\n");
      return 0;
   }

   // With palette size 1 (R=0, G=255, B=0) and pixel value 255, the output
   // should be palette[0] = (0, 255, 0) if clamped correctly.
   // With the bug, uninitialized stack data is read instead.
   // Check for the three common values:
   int is_correct = (img[0] == 0 && img[1] == 255 && img[2] == 0);
   int all_zero = (img[0] == 0 && img[1] == 0 && img[2] == 0);
   int all_max  = (img[0] == 255 && img[1] == 255 && img[2] == 255);
   // Truly correct value is (0, 255, 0)
   if (!is_correct) {
      printf("FAIL: Output (%d,%d,%d) differs from expected palette entry (0,255,0) - uninitialized memory leaked\n",
             img[0], img[1], img[2]);
      stbi_image_free(img);
      return 1;
   }

   printf("PASS: Output matches palette entry\n");
   stbi_image_free(img);
   return 0;
}
