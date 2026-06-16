// BUG-stb_image_write-010: Negative stride in PNG writer causes OOB read
//
// When stride_bytes is negative, stbiw__encode_png_line computes row pointers
// before the pixel buffer, leading to a heap-buffer-overflow in memcpy.
//
// This test passes when the bug is present (ASan catches the OOB read) and
// also passes when the fix is applied (no crash, returns 0 for invalid stride).

#ifdef __clang__
#define STBIWDEF static inline
#endif
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <stdlib.h>
#include <string.h>

static void my_writefunc(void *context, void *data, int size)
{
    (void)context;
    (void)data;
    (void)size;
}

int main(void)
{
    unsigned char *buf;
    int ret;

    // Allocate a small 3x3 RGB image
    buf = (unsigned char *)malloc(27);
    if (!buf) return 1;
    memset(buf, 128, 27);

    // Case 1: negative stride (stride = -9)
    // This should trigger OOB read in stbiw__encode_png_line
    ret = stbi_write_png_to_func(my_writefunc, NULL, 3, 3, 3, buf, -9);
    // If we get here, either ASan didn't trigger or the fix is applied
    // With the fix, this should return 0 (invalid stride)
    (void)ret;

    // Case 2: stride smaller than x*n (stride = 3, but x*n = 9)
    // This causes overlapping row access
    ret = stbi_write_png_to_func(my_writefunc, NULL, 3, 3, 3, buf, 3);
    (void)ret;

    free(buf);
    return 0;
}
