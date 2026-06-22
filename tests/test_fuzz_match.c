#define STBIR_ASSERT(x) ((void)0)
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main() {
    /* Matches what the fuzzer does for crash3 with api_mode=0 */
    unsigned char hdr[14] = {0x03,0x02,0x03,0x00,0x02,0x02,0x04,0x80,0x02,0x00,0x00,0x00,0x02,0x00};
    int input_w = (hdr[0] | (hdr[1] << 8)) & 0x3F;
    int input_h = (hdr[2] | (hdr[3] << 8)) & 0x3F;
    int output_scale_x = 1 + (hdr[4] & 0xF);
    int output_scale_y = 1 + (hdr[5] & 0xF);
    stbir_pixel_layout layout = (stbir_pixel_layout)(hdr[6] % 17);
    stbir_datatype dtype = (stbir_datatype)(hdr[7] % 6);
    stbir_edge edge = (stbir_edge)(hdr[8] % 4);
    stbir_filter flt = (stbir_filter)(hdr[9] % 7);
    int output_w = input_w * output_scale_x;
    int output_h = input_h * output_scale_y;
    int channels = 4;

    int type_size = (dtype == STBIR_TYPE_UINT16 || dtype == STBIR_TYPE_HALF_FLOAT) ? 2
                  : (dtype == STBIR_TYPE_FLOAT) ? 4 : 1;
    size_t in_bytes  = (size_t)input_w * (size_t)input_h * (size_t)channels * (size_t)type_size;
    size_t out_bytes = (size_t)output_w * (size_t)output_h * (size_t)channels * (size_t)type_size;

    printf("iw=%d ih=%d ow=%d oh=%d lay=%d dt=%d ed=%d fl=%d ts=%d\n",
           input_w, input_h, output_w, output_h, layout, dtype, edge, flt, type_size);

    void *in_buf  = malloc(in_bytes);
    void *out_buf = malloc(out_bytes);
    memset(in_buf, 0, in_bytes);
    memset(out_buf, 0, out_bytes);

    void *result = stbir_resize(in_buf, input_w, input_h, 0,
                                out_buf, output_w, output_h, 0,
                                layout, dtype, edge, flt);
    printf("result=%p\n", result);
    free(in_buf);
    free(out_buf);
    return 0;
}
