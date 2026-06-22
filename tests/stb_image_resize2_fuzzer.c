#ifdef __cplusplus
extern "C" {
#endif

#define STB_IMAGE_RESIZE2_IMPLEMENTATION
#define STBIR_ASSERT(x) ((void)0)  /* prevent assertion traps in fuzzer */

#include "stb_image_resize2.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
 * Fuzzer for stb_image_resize2.h.
 *
 * The first 14 bytes are a parameter header; the remaining bytes are
 * used as input pixel data (zero-padded if short).
 *
 * Byte layout of header:
 *   [0-1]   input_w        (little-endian uint16, then & 0x3F to cap at 63)
 *   [2-3]   input_h        (little-endian uint16, then & 0x3F)
 *   [4]     output_scale_x (1 + (byte & 0xF))   output_w = input_w * scale_x
 *   [5]     output_scale_y (1 + (byte & 0xF))   output_h = input_h * scale_y
 *   [6]     pixel_layout   (byte % 17, valid range 0..16)
 *   [7]     datatype       (byte % 6,  valid range 0..5)
 *   [8]     edge           (byte % 4,  valid range 0..3)
 *   [9]     filter         (byte % 7,  valid range 0..6)
 *   [10]    api_mode       (byte % 5,  0=stbir_resize, 1=uint8_srgb,
 *                                       2=uint8_linear, 3=float_linear, 4=extended)
 *   [11]    input_stride   (0 or 1: 0 means stride=0 (packed), 1 means stride=scanline)
 *   [12-13] <unused / padding>
 *
 * All dimensions are capped: input_w,input_h in [1..63], output_w,output_h in [1..256].
 * The pixel data buffer is sized to hold input_w * input_h * 4 * sizeof(float)
 * to cover the worst-case per-pixel consumption (float RGBA).
 */

#define MAX_INPUT_DIM  63
#define MAX_OUTPUT_DIM 256
#define MAX_CHANNELS   4

/* pixel channel counts for each stbir_pixel_layout */
static int pixel_chans[] = {
    3,  // STBIR_BGR = 0
    1,  // STBIR_1CHANNEL = 1
    2,  // STBIR_2CHANNEL = 2
    3,  // STBIR_RGB = 3
    4,  // STBIR_RGBA = 4
    4,  // STBIR_4CHANNEL = 5
    4,  // STBIR_BGRA = 6
    4,  // STBIR_ARGB = 7
    4,  // STBIR_ABGR = 8
    2,  // STBIR_RA = 9
    2,  // STBIR_AR = 10
    4,  // STBIR_RGBA_PM = 11
    4,  // STBIR_BGRA_PM = 12
    4,  // STBIR_ARGB_PM = 13
    4,  // STBIR_ABGR_PM = 14
    2,  // STBIR_RA_PM = 15
    2,  // STBIR_AR_PM = 16
};

static int channels_for_layout(stbir_pixel_layout layout) {
    unsigned int l = (unsigned int)layout;
    if (l < 17)
        return pixel_chans[l];
    return 4;
}

/* helper: clamp an int to [1, max] */
static int clamp1(int v, int max) {
    if (v < 1) return 1;
    if (v > max) return max;
    return v;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* need at least header */
    if (size < 14) return 0;

    /* ---- decode header ---- */
    int input_w         = (data[0] | (data[1] << 8)) & 0x3F;
    int input_h         = (data[2] | (data[3] << 8)) & 0x3F;
    int output_scale_x  = 1 + (data[4] & 0xF);
    int output_scale_y  = 1 + (data[5] & 0xF);
    stbir_pixel_layout layout  = (stbir_pixel_layout)(data[6] % 17);
    stbir_datatype      dtype = (stbir_datatype)(data[7] % 6);
    stbir_edge          edge  = (stbir_edge)(data[8] % 4);
    stbir_filter        flt   = (stbir_filter)(data[9] % 7);
    int api_mode               = data[10] % 5;
    int use_input_stride       = data[11] & 1;
    int output_w               = input_w * output_scale_x;
    int output_h               = input_h * output_scale_y;

    input_w  = clamp1(input_w,  MAX_INPUT_DIM);
    input_h  = clamp1(input_h,  MAX_INPUT_DIM);
    output_w = clamp1(output_w, MAX_OUTPUT_DIM);
    output_h = clamp1(output_h, MAX_OUTPUT_DIM);

    int channels = channels_for_layout(layout);

    /* create input pixel buffer (zero-filled, then copy what we can from fuzz data) */
    size_t input_bytes = (size_t)input_w * (size_t)input_h * (size_t)channels * sizeof(float);
    float *input_pixels = (float *)malloc(input_bytes);
    if (!input_pixels) return 0;
    memset(input_pixels, 0, input_bytes);

    size_t copy_bytes = size - 14;
    if (copy_bytes > input_bytes) copy_bytes = input_bytes;
    memcpy(input_pixels, data + 14, copy_bytes);

    /* create output buffer */
    size_t output_bytes = (size_t)output_w * (size_t)output_h * (size_t)channels * sizeof(float);
    float *output_pixels = (float *)malloc(output_bytes);
    if (!output_pixels) {
        free(input_pixels);
        return 0;
    }
    memset(output_pixels, 0, output_bytes);

    /* ---- exercise the API ---- */
    switch (api_mode) {
    case 0:
        /* stbir_resize - medium API (supports all datatypes) */
        {
            /* Allocate matching buffer for the datatype */
            int type_size = (dtype == STBIR_TYPE_UINT16 || dtype == STBIR_TYPE_HALF_FLOAT) ? 2
                          : (dtype == STBIR_TYPE_FLOAT) ? 4 : 1;
            size_t in_bytes  = (size_t)input_w * (size_t)input_h * (size_t)channels * (size_t)type_size;
            size_t out_bytes = (size_t)output_w * (size_t)output_h * (size_t)channels * (size_t)type_size;
            void *in_buf  = malloc(in_bytes);
            void *out_buf = malloc(out_bytes);
            if (in_buf && out_buf) {
                memset(in_buf, 0, in_bytes);
                memset(out_buf, 0, out_bytes);
                /* copy fuzz data into the type-appropriate buffer */
                size_t cpy = (size > 14) ? (size - 14) : 0;
                if (cpy > in_bytes) cpy = in_bytes;
                memcpy(in_buf, data + 14, cpy);
                int m_stride = use_input_stride ? (input_w * channels * type_size) : 0;
                stbir_resize(in_buf, input_w, input_h, m_stride,
                             out_buf, output_w, output_h, 0,
                             layout, dtype, edge, flt);
            }
            free(in_buf);
            free(out_buf);
        }
        break;
    case 1:
        /* stbir_resize_uint8_srgb - easy API */
        {
            size_t u8_input_bytes  = (size_t)input_w * (size_t)input_h * (size_t)channels;
            size_t u8_output_bytes = (size_t)output_w * (size_t)output_h * (size_t)channels;
            unsigned char *u8_input  = (unsigned char *)malloc(u8_input_bytes);
            unsigned char *u8_output = (unsigned char *)malloc(u8_output_bytes);
            if (u8_input && u8_output) {
                int u8_stride = use_input_stride ? (input_w * channels) : 0;
                memset(u8_input, 0, u8_input_bytes);
                memset(u8_output, 0, u8_output_bytes);
                if (copy_bytes > u8_input_bytes) copy_bytes = u8_input_bytes;
                memcpy(u8_input, data + 14, copy_bytes);
                stbir_resize_uint8_srgb(u8_input, input_w, input_h, u8_stride,
                                        u8_output, output_w, output_h, 0,
                                        layout);
            }
            free(u8_input);
            free(u8_output);
        }
        break;
    case 2:
        /* stbir_resize_uint8_linear - easy API */
        {
            size_t u8_input_bytes  = (size_t)input_w * (size_t)input_h * (size_t)channels;
            size_t u8_output_bytes = (size_t)output_w * (size_t)output_h * (size_t)channels;
            unsigned char *u8_input  = (unsigned char *)malloc(u8_input_bytes);
            unsigned char *u8_output = (unsigned char *)malloc(u8_output_bytes);
            if (u8_input && u8_output) {
                int u8_stride = use_input_stride ? (input_w * channels) : 0;
                memset(u8_input, 0, u8_input_bytes);
                memset(u8_output, 0, u8_output_bytes);
                size_t cpy = size - 14;
                if (cpy > u8_input_bytes) cpy = u8_input_bytes;
                memcpy(u8_input, data + 14, cpy);
                stbir_resize_uint8_linear(u8_input, input_w, input_h, u8_stride,
                                          u8_output, output_w, output_h, 0,
                                          layout);
            }
            free(u8_input);
            free(u8_output);
        }
        break;
    case 3:
        /* stbir_resize_float_linear - easy API */
        stbir_resize_float_linear(input_pixels, input_w, input_h, 0,
                                  output_pixels, output_w, output_h, 0,
                                  layout);
        break;
    case 4:
        /* Extended API via stbir_resize_init / stbir_resize_extended */
        {
            int ext_stride = use_input_stride ? (input_w * channels * (int)sizeof(float)) : 0;
            STBIR_RESIZE resize;
            memset(&resize, 0, sizeof(resize));
            stbir_resize_init(&resize,
                              input_pixels, input_w, input_h, ext_stride,
                              output_pixels, output_w, output_h, 0,
                              layout, dtype);
            stbir_set_edgemodes(&resize, edge, edge);
            stbir_set_filters(&resize, flt, flt);
            stbir_resize_extended(&resize);
            stbir_free_samplers(&resize);
        }
        break;
    }

    free(input_pixels);
    free(output_pixels);
    return 0;
}

#ifdef __cplusplus
}
#endif
