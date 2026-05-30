#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <stdlib.h>

int main(void)
{
   unsigned char img[] = { 255,0,0, 0,255,0, 0,0,255 };
   // quality=0 is documented as invalid (valid range: 1-100)
   // but no validation rejects it; should crash with ASan if bug exists
   stbi_write_jpg("/tmp/out_bug001.jpg", 3, 1, 3, img, 0);
   return 0;
}
