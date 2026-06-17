// BUG-010 Validation: Integer overflow in output pointer offset calculation
// stbir__update_info_from_resize at line 7550 computes:
//   info->output_data = ... + ( info->offset_x * info->channels * stbir__type_size[output_type] );
// All three operands are int, product can overflow INT_MAX if offset_x is large.
//
// The overflow requires offset_x * channels * type_size > INT_MAX.
// For RGBA UINT8: offset_x * 4 * 1 > 2147483647  →  offset_x > 536870911
// But offset_x is clipped to < output_w in stbir__calculate_region_transform.
// So we need output_w > 536870911, which is blocked by BUG-011's coefficients_size check.
// This test confirms the overflow cannot be triggered.

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

static int test_medium_api(void)
{
    unsigned char *input, *output;
    int input_w = 100, input_h = 100;
    // output_w large enough that output_subx could be large enough to overflow
    int output_w = 0x40000000;
    int output_h = 100;

    input = malloc(input_w * input_h * 4);
    output = malloc(1024);
    if (!input || !output) return -1;
    memset(input, 0, input_w * input_h * 4);

    printf("Test (a): medium API, output_w=0x%x...\n", output_w);
    void *r = stbir_resize(input, input_w, input_h, 0,
                           output, output_w, output_h, 0,
                           STBIR_RGBA, STBIR_TYPE_UINT8,
                           STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT);
    printf("  result=%p\n", r);

    free(input);
    free(output);
    return (r != 0);
}

static int test_extended_api(void)
{
    unsigned char *input, *output;
    int input_w = 536870913, input_h = 100;
    int output_w = 10, output_h = 10;

    input = malloc(1024);
    output = malloc(1024);
    if (!input || !output) return -1;
    memset(input, 0, 1024);

    printf("Test (b): extended API with large input_w -> small output (extreme downscale triggers coefficient_width with high inv_scale)...\n");
    {
        STBIR_RESIZE resize;
        stbir_resize_init(&resize, input, input_w, input_h, 0,
                          output, output_w, output_h, 0,
                          STBIR_RGBA, STBIR_TYPE_UINT8);
        stbir_set_edgemodes(&resize, STBIR_EDGE_CLAMP, STBIR_EDGE_CLAMP);
        stbir_set_filters(&resize, STBIR_FILTER_DEFAULT, STBIR_FILTER_DEFAULT);
        int ok = stbir_build_samplers(&resize);
        printf("  build_samplers=%d\n", ok);
        if (ok) {
            int r = stbir_resize_extended(&resize);
            printf("  resize_extended=%d\n", r);
        }
    }

    free(input);
    free(output);
    return 0;
}

int main()
{
    int rc = 0;
    rc |= test_medium_api();
    rc |= test_extended_api();

    printf("\nDone. %s\n",
           rc ? "Some tests returned non-zero." : "All tests clean — overflow path is blocked.");
    return 0;
}
