#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

int main()
{
    unsigned char* input;
    unsigned char* output;
    int input_w = 10, input_h = 10;
    int output_w = 20, output_h = 20;

    input = malloc(input_w * input_h * 4);
    output = malloc(output_w * output_h * 4);
    if (!input || !output) {
        printf("FAIL: malloc failed\n");
        return 1;
    }
    memset(input, 0, input_w * input_h * 4);

    // Use the extended API to test stbir_set_edgemodes with invalid edge values
    STBIR_RESIZE resize;
    stbir_resize_init(&resize,
        input, input_w, input_h, 0,
        output, output_w, output_h, 0,
        STBIR_RGBA, STBIR_TYPE_UINT8);

    // BUG-005: stbir_set_edgemodes does not validate edge enum values
    // Passing (stbir_edge)7 should be rejected but currently returns success
    int ret = stbir_set_edgemodes(&resize, (stbir_edge)7, (stbir_edge)7);
    if (ret) {
        // set_edgemodes returned success for invalid edges - this is BUG-005
        printf("stbir_set_edgemodes accepted invalid edge value (BUG-005 confirmed)\n");
    } else {
        // If it returns 0, the bug is already fixed
        printf("stbir_set_edgemodes rejected invalid edge value (BUG-005 already fixed)\n");
        free(input);
        free(output);
        return 0;
    }

    // Build samplers - should fail due to BUG-004 fix (edge validated in perform_build)
    ret = stbir_build_samplers(&resize);
    if (ret) {
        printf("BUG-005: build_samplers succeeded with invalid edges — BUG-004 fix not active\n");
        free(input);
        free(output);
        return 1;
    }

    printf("BUG-005 validated: stbir_set_edgemodes accepts invalid edges but build catches them\n");
    free(input);
    free(output);
    return 0;
}
