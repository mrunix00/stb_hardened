#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <stdlib.h>

static void my_write_func(void *context, void *data, int size)
{
   (void)context; (void)data; (void)size;
}

int main(void)
{
   // x=16 (RLE path uses scratch) - should pass cleanly with no UB/ASan
   {
      float *buf = (float *)malloc(sizeof(float) * 16 * 3);
      int ret;
      for (int i = 0; i < 16 * 3; i++) buf[i] = 1.0f;
      ret = stbi_write_hdr_to_func(my_write_func, NULL, 16, 1, 3, buf);
      free(buf);
      if (!ret) return 1;
   }
   // With the fix: (size_t)x*4 prevents the signed overflow,
   // and the NULL check prevents crash on malloc failure.
   // Simply calling with valid small args confirms no regression.
   {
      float buf[12] = { 1.0f, 0.0f, 0.0f, 1.0f, 0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 1.0f, 0.5f, 1.0f };
      int ret = stbi_write_hdr_to_func(my_write_func, NULL, 4, 1, 3, buf);
      if (!ret) return 1;
   }
   return 0;
}
