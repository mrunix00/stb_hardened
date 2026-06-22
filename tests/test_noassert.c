#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main() {
    unsigned char *in_buf = malloc(3*3*4);
    unsigned char *out_buf = malloc(9*9*4);
    memset(in_buf, 0, 3*3*4);
    memset(out_buf, 0, 9*9*4);
    void *result = stbir_resize(in_buf, 3, 3, 0,
                                out_buf, 9, 9, 0,
                                STBIR_RGBA, STBIR_TYPE_UINT8,
                                STBIR_EDGE_WRAP, STBIR_FILTER_DEFAULT);
    printf("result=%p\n", result);
    free(in_buf); free(out_buf);
    return 0;
}
