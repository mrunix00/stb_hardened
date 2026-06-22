// BUG-017 Validation: OOB table index in stbir__encode_uint8_srgb SIMD path
//
// The SIMD encode path uses stbir__min_max_shift20 to clamp float values
// and compute a table index for fp32_to_srgb8_tab4[104]. When STBIR_ASSERT
// is disabled, the encode function receives out-of-range float values
// that produce an OOB index (-912) into the table.
//
// NOTE: This bug only manifests when the implementation is compiled in a
// separate translation unit from the caller (two-TU build). The fuzzer
// source file is used as the implementation TU.

#include <stdint.h>
#include <stdio.h>

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

int main()
{
    // crash3.bin: 3x3 RGBA UINT8 -> 9x9, WRAP edge, DEFAULT filter
    unsigned char hdr[14] = {
        0x03, 0x02,  // input_w = 3
        0x03, 0x00,  // input_h = 3
        0x02,        // output_scale_x = 3 -> output_w = 9
        0x02,        // output_scale_y = 3 -> output_h = 9
        0x04,        // pixel_layout = 4 (STBIR_RGBA)
        0x80,        // datatype = 128%6 = 2 (STBIR_TYPE_UINT8)
        0x02,        // edge = 2 (STBIR_EDGE_WRAP)
        0x00,        // filter = 0 (STBIR_FILTER_DEFAULT)
        0x00,        // api_mode = 0 (stbir_resize)
        0x00,        // input_stride = 0 (packed)
        0x00, 0x00   // padding
    };

    printf("BUG-017: 3x3->9x9 RGBA UINT8 WRAP DEFAULT...\n");

    // Expected: UBSan reports "index -912 out of bounds for type
    //           'const stbir_uint32[104]'"
    LLVMFuzzerTestOneInput(hdr, 14);

    printf("  If no UBSan OOB error was reported, the bug could not be reproduced.\n");
    return 0;
}
