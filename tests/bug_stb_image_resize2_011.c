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

    // output_w=20000, input_w=200000000 → scale=0.0001
    // coefficient_width ~ 4.0/0.0001 = 40000 (from stbir__get_coefficient_width case 2)
    // num_contributors=20000 → 20000*40000*4=3.2G > INT_MAX → signed overflow at line 6670
    int input_w = 200000000;
    int input_h = 1;
    int output_w = 20000;
    int output_h = 1;

    input = malloc(1024);
    output = malloc(output_w * 4);
    if (!input || !output) {
        printf("FAIL: malloc failed\n");
        return 1;
    }
    memset(input, 0, 1024);

    stbir_resize_init(&resize,
        input, input_w, input_h, 0,
        output, output_w, output_h, output_w * 4,
        STBIR_RGBA, STBIR_TYPE_UINT8);

    if (!stbir_resize_extended(&resize))
    {
        printf("Resize failed as expected\n");
        free(input);
        free(output);
        return 0;
    }

    printf("Resize succeeded (unexpected)\n");
    free(input);
    free(output);
    return 1;
}
