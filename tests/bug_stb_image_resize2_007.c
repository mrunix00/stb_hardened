#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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

    // Pass invalid datatype (255) - stbir__type_size[] has only 6 entries (0-5).
    // The OOB read at line 8113 causes UBSan to fire.
    void* result = stbir_resize(
        input, input_w, input_h, 0,
        output, output_w, output_h, 0,
        STBIR_RGBA, (stbir_datatype)255,
        STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT);

    printf("Resize returned: %p\n", result);
    free(input);
    free(output);
    return result ? 1 : 0;
}
