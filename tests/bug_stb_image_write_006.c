#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <stdlib.h>

static void my_write_func(void *context, void *data, int size)
{
   (void)context; (void)data; (void)size;
}

int main(void)
{
   unsigned char buf[64] = {0};
   // comp=-1 in TGA RLE writer: memcmp(begin, row+(i+1)*comp, comp) with x=2
   // gives memcmp(..., -1) → huge size_t → OOB read
   stbi_write_tga_to_func(my_write_func, NULL, 2, 1, -1, buf);
   return 0;
}
