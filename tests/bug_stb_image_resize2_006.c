#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

int main()
{
    unsigned char* input;
    unsigned char* output;
    STBIR_RESIZE resize;
    int input_w = 100, input_h = 100;
    int output_w = 10, output_h = 10;

    input = malloc(input_w * input_h * 4);
    output = malloc(output_w * output_h * 4);
    if (!input || !output) {
        printf("FAIL: malloc failed\n");
        return 1;
    }
    memset(input, 0, input_w * input_h * 4);

    stbir_resize_init(&resize,
        input, input_w, input_h, 0,
        output, output_w, output_h, 0,
        STBIR_RGBA, STBIR_TYPE_UINT8);

    // Set invalid pixel layout (99) - the lookup array
    // stbir__pixel_layout_convert_public_to_internal[] has only 17 entries (0-16).
    // This OOB read propagates to channel counts and allocations.
    stbir_set_pixel_layouts(&resize, (stbir_pixel_layout)99, (stbir_pixel_layout)99);

    if (!stbir_resize_extended(&resize))
    {
        printf("Resize failed as expected (OOB was prevented or caught)\n");
        free(input);
        free(output);
        return 0;
    }

    printf("FAIL: resize succeeded despite invalid pixel layout\n");
    free(input);
    free(output);
    return 1;
}
