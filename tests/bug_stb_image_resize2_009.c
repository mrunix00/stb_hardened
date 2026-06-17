// BUG-009 Validation: Integer overflow in default stride computation
// stbir__update_info_from_resize at line 7547 computes:
//   info->output_stride_bytes = info->channels * info->horizontal.scale_info.output_sub_size * stbir__type_size[output_type];
// All three operands are int, product can overflow INT_MAX.
//
// Three approaches to trigger:
//  (a) medium API with output_w large enough to overflow 4 * output_w * 1
//  (b) extended API with output_stride_in_bytes=0 (triggers auto-stride)
//  (c) extended API with zero stride and point_sample filter (lowest coefficient cost)

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
    int output_w = 0x40000000;  // triggers overflow: 4 * 0x40000000 * 1 = 0x100000000 > INT_MAX
    int output_h = 100;

    input = malloc(input_w * input_h * 4);
    output = malloc(1024); // small, test may not reach actual write
    if (!input || !output) return -1;
    memset(input, 0, input_w * input_h * 4);

    printf("Test (a): medium API with output_w=0x%x...\n", output_w);
    void *r = stbir_resize(input, input_w, input_h, 0,
                           output, output_w, output_h, 0,
                           STBIR_RGBA, STBIR_TYPE_UINT8,
                           STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT);
    printf("  result=%p\n", r);

    free(input);
    free(output);
    return (r != 0) ? 1 : 0;
}

static int test_extended_api(void)
{
    unsigned char *input, *output;
    int input_w = 100, input_h = 100;
    int output_w = 0x40000000;
    int output_h = 100;

    input = malloc(input_w * input_h * 4);
    output = malloc(1024);
    if (!input || !output) return -1;
    memset(input, 0, input_w * input_h * 4);

    printf("Test (b): extended API with output_stride=0, output_w=0x%x...\n", output_w);
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

static int test_extended_point(void)
{
    unsigned char *input, *output;
    int input_w = 100, input_h = 100;
    int output_w = 0x40000000;
    int output_h = 100;

    input = malloc(input_w * input_h * 4);
    output = malloc(1024);
    if (!input || !output) return -1;
    memset(input, 0, input_w * input_h * 4);

    printf("Test (c): extended API, point_sample, output_w=0x%x...\n", output_w);
    {
        STBIR_RESIZE resize;
        stbir_resize_init(&resize, input, input_w, input_h, 0,
                          output, output_w, output_h, 0,
                          STBIR_RGBA, STBIR_TYPE_UINT8);
        stbir_set_edgemodes(&resize, STBIR_EDGE_CLAMP, STBIR_EDGE_CLAMP);
        stbir_set_filters(&resize, STBIR_FILTER_POINT_SAMPLE, STBIR_FILTER_POINT_SAMPLE);
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
    rc |= test_extended_point();

    if (rc)
        printf("\nSome tests returned non-zero (build may have rejected the request before overflow)\n");

    printf("\nDone. If UBSan reported a signed integer overflow, BUG-009 is validated.\n"
           "If all tests ran without UBSan warnings, the overflow path is already blocked by earlier checks.\n");
    return 0;
}
