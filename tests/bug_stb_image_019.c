// BUG-stb_image-019: BMP palette uninitialized stack read
// When psize < 256, pixel values >= psize read uninitialized stack
// from the pal[256][4] array.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#include "stb_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 8-bit BMP: 1x1, 1 palette entry (R=0,G=255,B=0), pixel value=255
unsigned char test_bmp[] = {
  0x42, 0x4d, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3a, 0x00, 0x00, 0x00, 0x28, 0x00,
  0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x08, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
};

int main(void)
{
   int x, y, comp;
   unsigned char *img = stbi_load_from_memory(test_bmp, sizeof(test_bmp), &x, &y, &comp, 0);
   if (!img) {
      printf("PASS: Corrupt BMP rejected\n");
      return 0;
   }

   // With 1 palette entry (R=0,G=255,B=0) and pixel value 255, output
   // should be palette[0] = (0,255,0) if clamped. With the bug,
   // uninitialized stack data is read.
   int is_correct = (img[0] == 0 && img[1] == 255 && img[2] == 0);
   if (!is_correct) {
      printf("FAIL: Output (%d,%d,%d) differs from palette entry (0,255,0) - uninitialized memory leaked\n",
             img[0], img[1], img[2]);
      stbi_image_free(img);
      return 1;
   }

   printf("PASS: Output matches palette entry\n");
   stbi_image_free(img);
   return 0;
}
