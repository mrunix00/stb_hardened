// BUG-013 Validation: Float Underflow → Division by Zero / Infinite Scale
//
// stbir__calculate_region_transform line 7734:
//   scale = ( output_range / input_range ) * ratio;
// When input_range >> output_range, output_range/input_range underflows to 0.0,
// then inv_scale = 1.0/0.0 = +inf, causing downstream chaos.
//
// Test: input_w = INT_MAX (0x7fffffff), output_w = 1

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

int main()
{
    unsigned char *input, *output;
    int input_w = 0x7fffffff, input_h = 100;
    int output_w = 1, output_h = 100;

    input = malloc(256);
    output = malloc(256);
    if (!input || !output) return -1;
    memset(input, 0, 256);

    printf("BUG-013: input_w=0x%x -> output_w=%d\n", input_w, output_w);

    // Medium API
    void *r = stbir_resize(input, input_w, input_h, 0,
                            output, output_w, output_h, 0,
                            STBIR_RGBA, STBIR_TYPE_UINT8,
                            STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT);
    printf("  medium API result=%p\n", r);

    // Extended API
    {
        STBIR_RESIZE resize;
        stbir_resize_init(&resize, input, input_w, input_h, 0,
                          output, output_w, output_h, 0,
                          STBIR_RGBA, STBIR_TYPE_UINT8);
        stbir_set_edgemodes(&resize, STBIR_EDGE_CLAMP, STBIR_EDGE_CLAMP);
        stbir_set_filters(&resize, STBIR_FILTER_DEFAULT, STBIR_FILTER_DEFAULT);
        int ok = stbir_build_samplers(&resize);
        printf("  extended API build_samplers=%d\n", ok);
        if (ok) {
            int r2 = stbir_resize_extended(&resize);
            printf("  extended API resize_extended=%d\n", r2);
        }
    }

    free(input);
    free(output);
    printf("\nDone. If no UBSan float underflow/infinity reported, the bug is non-trivial to trigger.\n");
    return 0;
}
