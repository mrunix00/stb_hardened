#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <stdlib.h>

int main(void)
{
   unsigned char img[64] = {0};
   // negative width - should be rejected but isn't
   int ret = stbi_write_jpg("/tmp/out_bug004.jpg", -1, 8, 3, img, 90);
   // If validation is missing, this will either crash or produce garbage
   (void)ret;
   return 0;
}
