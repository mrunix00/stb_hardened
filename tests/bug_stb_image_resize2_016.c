// BUG-016 Validation: Misaligned int store in non-SIMD stbir__encode_uint8_linear
// 
// Line 8744: *(int*)(output-4) = stbir__simdi_to_int( i0 );
// When output pointer is not 4-byte aligned (e.g., 3-channel BGR with packed stride),
// this cast from unsigned char* to int* is UB (UBSan violation).
//
// Test: BGR 3-channel resize where output stride = 2*3 = 6 (not a multiple of 4).
// Even-numbered scanlines land on addresses not 4-byte aligned.

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

int main()
{
    unsigned char input[2*2*3];
    unsigned char output[2*2*3];
    void *result;
    int i;

    // Fill input with distinguishable values
    for (i = 0; i < 2*2*3; i++)
        input[i] = (unsigned char)(i * 10 + 50);

    memset(output, 0, sizeof(output));

    printf("BUG-016: BGR 3-channel resize (packed stride not multiple of 4)...\n");

    result = stbir_resize(input, 2, 2, 0,
                          output, 2, 2, 0,
                          STBIR_BGR, STBIR_TYPE_UINT8,
                          STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT);

    printf("  result=%p\n", result);
    if (result) {
        printf("  output[0..5] = %02x %02x %02x %02x %02x %02x\n",
               output[0], output[1], output[2], output[3], output[4], output[5]);
    }

    printf("\nIf no UBSan misaligned access error was reported, the bug is non-trivial to trigger.\n");
    return 0;
}
