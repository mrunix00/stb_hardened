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

    // Set invalid edge mode (42) - stbir__edge_wrap_slow[] has only 4 entries (0-3)
    // This will cause an OOB function pointer read during build.
    stbir_set_edgemodes(&resize, (stbir_edge)42, (stbir_edge)42);

    // Trigger the build. stbir__perform_build validates filter enums
    // but does NOT validate edge enums, so the invalid value 42 reaches
    // stbir__get_extents -> stbir__edge_wrap -> stbir__edge_wrap_slow[42]
    if (!stbir_resize_extended(&resize))
    {
        printf("Resize returned failure (expected OOB crash was sanitized or edge was validated)\n");
        free(input);
        free(output);
        return 0;
    }

    printf("FAIL: resize succeeded despite invalid edge mode - no OOB detected\n");
    free(input);
    free(output);
    return 1;
}
