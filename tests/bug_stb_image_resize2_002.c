#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

int main()
{
    int input_w = 10;
    int input_h = 10;
    int output_w = 0x40000001;
    int output_h = 10;

    unsigned char* input = malloc(input_w * input_h * 4);
    if (!input) return 0;
    memset(input, 0, input_w * input_h * 4);

    printf("Attempting resize with large width: %dx%d -> %dx%d\n", input_w, input_h, output_w, output_h);

    void* result = stbir_resize(
        input, input_w, input_h, 0,
        NULL, output_w, output_h, 0,
        STBIR_RGBA, STBIR_TYPE_UINT8, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT
    );

    if (result == NULL) {
        printf("Resize failed as expected (or due to allocation failure).\n");
    } else {
        printf("Resize succeeded? result=%p\n", result);
        STBIR_FREE(result, NULL);
    }

    free(input);
    return 0;
}
