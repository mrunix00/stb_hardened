#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

int main()
{
    unsigned char* input;
    unsigned char* output;
    int input_w = 100, input_h = 100;
    int output_w = 10, output_h = 10;

    input = malloc(input_w * input_h * 4);
    output = malloc(output_w * output_h * 4);
    if (!input || !output) {
        printf("FAIL: malloc failed\n");
        return 1;
    }
    memset(input, 0, input_w * input_h * 4);

    // output_stride_in_bytes = INT_MIN causes -INT_MIN to overflow
    // at line 8131: positive_output_stride_in_bytes = -positive_output_stride_in_bytes
    void* result = stbir_resize(
        input, input_w, input_h, 0,
        output, output_w, output_h, INT_MIN,
        STBIR_RGBA, STBIR_TYPE_UINT8,
        STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT);

    printf("Resize returned: %p\n", result);
    free(input);
    free(output);
    return 0;
}
