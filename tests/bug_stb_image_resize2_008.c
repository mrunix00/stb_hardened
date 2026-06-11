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

    // Step 1: Init and build samplers with valid types
    // Use stbir_build_samplers so samplers remain allocated
    STBIR_RESIZE resize;
    stbir_resize_init(&resize,
        input, input_w, input_h, 0,
        output, output_w, output_h, 0,
        STBIR_RGBA, STBIR_TYPE_UINT8);

    if (!stbir_build_samplers(&resize)) {
        printf("FAIL: build_samplers failed\n");
        free(input);
        free(output);
        return 1;
    }

    printf("Build succeeded. samplers=%p needs_rebuild=%d\n",
           (void*)resize.samplers, resize.needs_rebuild);

    // Step 2: Change datatype to an invalid value.
    // stbir_set_datatypes calls stbir__update_info_from_resize directly
    // (bypassing stbir__perform_build's validation), which indexes
    // stbir__type_size[invalid_type] and decode_simple[invalid_type].
    printf("Calling stbir_set_datatypes with invalid type 99...\n");
    stbir_set_datatypes(&resize, (stbir_datatype)99, (stbir_datatype)99);

    // If we reach here, the OOB access may have been optimized away
    // or ASan didn't catch it. Try resize_extended to exercise the
    // corrupted function pointers.
    printf("Calling resize_extended with corrupted info...\n");
    int result = stbir_resize_extended(&resize);
    printf("BUG-008: resize_extended returned %d\n", result);

    stbir_free_samplers(&resize);
    free(input);
    free(output);
    return 0;
}
