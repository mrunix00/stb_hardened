#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

int main()
{
    int input_w = 232;
    int input_h = 232;
    int output_w = 302;
    int output_h = 302;
    unsigned char* input = malloc(input_w * input_h * 4);
    if (!input) return 0;
    memset(input, 0, input_w * input_h * 4);
    unsigned char* output = malloc(output_w * output_h * 4);
    if (!output) {
        free(input);
        return 0;
    }
    printf("Starting resize 232x232 -> 302x302 (Point Sampling)...\n");
    stbir_resize(
        input, input_w, input_h, 0,
        output, output_w, output_h, 0,
        STBIR_RGBA, STBIR_TYPE_UINT8, STBIR_EDGE_CLAMP, STBIR_FILTER_POINT_SAMPLE
    );
    printf("Finished resize.\n");

    free(input);
    free(output);
    return 0;
}
