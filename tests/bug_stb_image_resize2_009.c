#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

int main()
{
    unsigned char* input;
    unsigned char* output;
    STBIR_RESIZE resize;

    // input_w large enough to make 4 * input_w * 1 overflow int at line 7533
    // output_w is moderate so output stride doesn't overflow
    // Pass output_stride_in_bytes explicitly to bypass output stride computation
    int input_w = 600000000;
    int input_h = 1;
    int output_w = 10;
    int output_h = 1;

    input = malloc(1024);
    output = malloc(1024);
    if (!input || !output) {
        printf("FAIL: malloc failed\n");
        return 1;
    }
    memset(input, 0, 1024);

    stbir_resize_init(&resize,
        input, input_w, input_h, 0,
        output, output_w, output_h, 40,
        STBIR_RGBA, STBIR_TYPE_UINT8);

    // Trigger build; input_stride_in_bytes == 0 causes stride computation
    // at line 7533: info->channels * info->horizontal.scale_info.input_full_size * stbir__type_size[input_type]
    // which overflows int.
    if (!stbir_resize_extended(&resize))
    {
        printf("Resize failed as expected (overflow was caught)\n");
        free(input);
        free(output);
        return 0;
    }

    printf("Resize succeeded (unexpected - no overflow detected)\n");
    free(input);
    free(output);
    return 1;
}
