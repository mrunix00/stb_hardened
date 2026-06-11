#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

static unsigned char dummy_pixel[4] = {0};

static const void* input_cb(void* optional_output, const void* input_ptr, int num_pixels, int x, int y, void* context)
{
    return dummy_pixel;
}

static void output_cb(const void* output_ptr, int num_pixels, int y, void* context)
{
}

int main()
{
    // Extreme downscale: 2e9 x 1 -> 1 x 1 with Mitchell filter.
    // scale = 1/2e9 = 5e-10, coefficient_width = ceil(support(scale) * 2 / scale)
    //          = ceil(2.0 * 2.0 / 5e-10) = ceil(8e9) = 8e9
    // (int)8e9 is undefined behavior -- UBSan should catch the float-to-int overflow.
    STBIR_RESIZE resize;
    stbir_resize_init(&resize,
        NULL, 2000000000, 1, 16,
        NULL, 1, 1, 16,
        STBIR_RGBA, STBIR_TYPE_UINT8);

    stbir_set_pixel_callbacks(&resize, input_cb, output_cb);

    // Manually trigger the filter pixel width computation to verify UB
    #ifdef STB_IMAGE_RESIZE_IMPLEMENTATION
    // Direct call to the static function via the resize path
    printf("Calling build_samplers with extreme downscale ratio...\n");
    printf("input_w=%d output_w=%d\n", resize.input_w, resize.output_w);
    printf("horizontal_filter=%d\n", resize.horizontal_filter);
    fflush(stdout);
    #endif

    int result = stbir_build_samplers(&resize);
    printf("build_samplers returned: %d\n", result);

    if (resize.samplers) {
        stbir_free_samplers(&resize);
    }
    return 0;
    return 0;
}
