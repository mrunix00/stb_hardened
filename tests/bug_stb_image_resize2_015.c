// BUG-015 Validation: Misaligned uint64 store in STBIR_MOVE_2
// 
// stbir__pack_coefficients case 3: when widest == 3 and coefficient_width != 3,
// the STBIR_MOVE_2 macro at line 3746 casts float* to stbir_uint64* and writes
// 8 bytes. After the first iteration pc advances by 12 bytes (3 floats), landing
// on an address misaligned for uint64 (12 mod 8 = 4).
//
// Test: upscale from 3x3 to 9x9 with cubic filter and reflect edge mode.
// This should produce coefficient_width > widest for some contributors,
// triggering the pack path.

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

int main()
{
    unsigned char input[3*3*4];
    unsigned char output[9*9*4];
    void *result;
    int i;

    // Fill input with distinguishable values
    for (i = 0; i < 3*3*4; i++)
        input[i] = (unsigned char)(i * 20 + 10);

    memset(output, 0, sizeof(output));

    printf("BUG-015: 3x3 -> 9x9 RGBA with CUBICBSPLINE + REFLECT edge...\n");

    result = stbir_resize(input, 3, 3, 0,
                          output, 9, 9, 0,
                          STBIR_RGBA, STBIR_TYPE_UINT8,
                          STBIR_EDGE_REFLECT, STBIR_FILTER_CUBICBSPLINE);

    printf("  result=%p\n", result);

    result = stbir_resize(input, 3, 3, 0,
                          output, 9, 9, 0,
                          STBIR_RGBA, STBIR_TYPE_UINT8,
                          STBIR_EDGE_CLAMP, STBIR_FILTER_CATMULLROM);

    printf("  catmullrom result=%p\n", result);

    printf("\nIf no UBSan misaligned access error was reported, the bug is non-trivial to trigger.\n");
    return 0;
}
