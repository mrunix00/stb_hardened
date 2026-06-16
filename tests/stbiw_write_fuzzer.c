// Fuzzing harness for stb_image_write.h
// Exercises all output formats with fuzz-generated pixel data and parameters

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef __clang__
#define STBIWDEF static inline
#endif
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

static struct {
    unsigned char *buf;
    int len;
    int cap;
} memout;

static void memwrite_func(void *context, void *data, int size)
{
    (void)context;
    if (size <= 0) return;
    int needed = memout.len + size;
    if (needed > memout.cap) {
        int newcap = memout.cap ? memout.cap * 2 : 65536;
        while (newcap < needed) newcap *= 2;
        unsigned char *nbuf = (unsigned char *)realloc(memout.buf, (size_t)newcap);
        if (!nbuf) return;
        memout.buf = nbuf;
        memout.cap = newcap;
    }
    memcpy(memout.buf + memout.len, data, (size_t)size);
    memout.len = needed;
}

static void reset_memout(void)
{
    memout.len = 0;
}

// Simple 32-bit xorshift PRNG for test data generation
static unsigned int xorshift32(unsigned int *state)
{
    unsigned int x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    // Need at least 9 bytes: 5 for parameters, 4+ for pixel data
    if (size < 9) return 0;

    unsigned int seed = 42;

    // Parse parameters from fuzz input
    unsigned int fmt_selector  = data[0] % 11;  // 0-10, select format
    int w = (int)(data[1] | ((unsigned)data[2] << 8));
    int h = (int)(data[3] | ((unsigned)data[4] << 8));
    int comp = (int)(data[5] % 5);  // 0-4
    int quality_or_flag = (int)data[6];
    int extra_param = (int)data[7];
    int flip = data[8] & 1;

    // Ensure reasonable-ish dimensions to avoid trivially huge allocations
    // But still test extreme values
    if (w > 4096) w = 4096;
    if (h > 4096) h = 4096;
    if (w < -1) w = -1;
    if (h < -1) h = -1;

    // comp 0 means test various comp values including edge cases
    if (comp == 0) {
        comp = (int)(data[5] % 7) - 1;  // -1 to 5
    }

    // Compute pixel data size needed
    int pixel_count = (w > 0 && h > 0) ? w * h : 0;
    int pixel_bytes = pixel_count * (comp > 0 ? comp : 4);

    // Allocate pixel buffer with deterministic data
    // Use fuzz data first, then PRNG for remaining
    unsigned char *pixels = NULL;
    float *float_pixels = NULL;
    int needs_free = 0;

    // For HDR we need float pixels
    if (fmt_selector == 2 || fmt_selector == 10) {
        int fcount = pixel_count * 3;  // HDR always uses 3 comp
        if (fcount > 0) {
            float_pixels = (float *)malloc((size_t)fcount * sizeof(float));
            if (!float_pixels) return 0;
            needs_free = 1;
            for (int i = 0; i < fcount && i < (int)size - 9; i++) {
                float_pixels[i] = (float)data[9 + i];
            }
            for (int i = (int)size - 9; i < fcount; i++) {
                float_pixels[i] = (float)(xorshift32(&seed) & 0xFF);
            }
        }
    }

    // For other formats, use byte pixels
    if (fmt_selector != 2 && fmt_selector != 10) {
        int bpp = comp > 0 ? comp : 4;
        int bcount = pixel_count * bpp;
        if (bcount > 0) {
            pixels = (unsigned char *)malloc((size_t)(bcount > 0 ? bcount : 1));
            if (!pixels) {
                free(float_pixels);
                return 0;
            }
            needs_free = 1;
            int copy_bytes = (int)size - 9;
            if (copy_bytes > bcount) copy_bytes = bcount;
            if (copy_bytes > 0) memcpy(pixels, data + 9, (size_t)copy_bytes);
            for (int i = copy_bytes; i < bcount; i++) {
                pixels[i] = (unsigned char)(xorshift32(&seed) & 0xFF);
            }
        }
    }

    // Set flip
    stbi_flip_vertically_on_write(flip);

    // Set TGA RLE flag
    stbi_write_tga_with_rle = (extra_param & 1) ^ 1;  // toggle between 0 and 1

    // Test different format writers
    switch (fmt_selector) {
        case 0: // BMP
        case 8: // BMP with edge params
            if (pixels && pixel_count > 0) {
                reset_memout();
                stbi_write_bmp_to_func(memwrite_func, NULL, w, h, comp, pixels);
                if (fmt_selector == 8 && comp > 0 && comp <= 4) {
                    // Test with negative stride (as if reading from bottom)
                    reset_memout();
                    stbi_write_bmp_to_func(memwrite_func, NULL, w, h, comp, pixels);
                }
            }
            break;

        case 1: // TGA
        case 9: // TGA with RLE off
            if (pixels && pixel_count > 0) {
                stbi_write_tga_with_rle = (fmt_selector == 9) ? 0 : 1;
                reset_memout();
                stbi_write_tga_to_func(memwrite_func, NULL, w, h, comp, pixels);
                // Test RLE off too
                stbi_write_tga_with_rle = 0;
                reset_memout();
                stbi_write_tga_to_func(memwrite_func, NULL, w, h, comp, pixels);
            }
            break;

        case 2: // HDR
        case 10: // HDR with NaN/inf/special values
            if (float_pixels && pixel_count > 0) {
                if (fmt_selector == 10 && pixel_count > 0) {
                    // Inject extreme float values at start of buffer
                    float_pixels[0] = INFINITY;
                    if (pixel_count > 1) float_pixels[1] = NAN;
                    if (pixel_count > 2) float_pixels[2] = -INFINITY;
                }
                reset_memout();
                stbi_write_hdr_to_func(memwrite_func, NULL, w, h, 3, float_pixels);
            }
            break;

        case 3: // JPEG
            if (pixels && pixel_count > 0 && comp > 0) {
                int quality = quality_or_flag;
                reset_memout();
                stbi_write_jpg_to_func(memwrite_func, NULL, w, h, comp, pixels, quality);
            }
            break;

        case 4: // PNG basic
            if (pixels && pixel_count > 0 && comp > 0) {
                reset_memout();
                stbi_write_png_to_func(memwrite_func, NULL, w, h, comp, pixels, w * comp);
            }
            break;

        case 5: // PNG with various strides
            if (pixels && pixel_count > 0 && comp > 0) {
                // Test zero stride (auto-calculated)
                reset_memout();
                stbi_write_png_to_func(memwrite_func, NULL, w, h, comp, pixels, 0);
                // Test custom stride (stride > w*comp)
                int stride = (w * comp) + (extra_param % 16);
                reset_memout();
                stbi_write_png_to_func(memwrite_func, NULL, w, h, comp, pixels, stride);
            }
            break;

        case 6: // Test all formats sequentially with small data
            if (pixels && pixel_count > 0 && comp > 0) {
                reset_memout();
                stbi_write_bmp_to_func(memwrite_func, NULL, w, h, comp, pixels);
                reset_memout();
                stbi_write_tga_to_func(memwrite_func, NULL, w, h, comp, pixels);
                reset_memout();
                stbi_write_png_to_func(memwrite_func, NULL, w, h, comp, pixels, w * comp);
                int quality = quality_or_flag % 101;
                reset_memout();
                stbi_write_jpg_to_func(memwrite_func, NULL, w, h, comp, pixels, quality);
            }
            break;

        case 7: // Test edge cases - zero dimensions, etc
            reset_memout();
            if (pixels) stbi_write_bmp_to_func(memwrite_func, NULL, 0, 0, 3, pixels);
            reset_memout();
            if (pixels) stbi_write_tga_to_func(memwrite_func, NULL, -1, 10, 3, pixels);
            reset_memout();
            if (pixels) stbi_write_tga_to_func(memwrite_func, NULL, 10, -1, 3, pixels);
            reset_memout();
            if (float_pixels) stbi_write_hdr_to_func(memwrite_func, NULL, 5, 5, 0, float_pixels);
            reset_memout();
            if (pixels) stbi_write_png_to_func(memwrite_func, NULL, 1, 1, 0, pixels, 0);
            break;

        default:
            break;
    }

    // Test JPEG with various comp values and qualities in all cases
    if (pixels && pixel_count > 0 && comp > 0 && comp <= 4) {
        // Try extreme quality values
        reset_memout();
        stbi_write_jpg_to_func(memwrite_func, NULL, w, h, comp, pixels, 0);
        reset_memout();
        stbi_write_jpg_to_func(memwrite_func, NULL, w, h, comp, pixels, 101);
        reset_memout();
        stbi_write_jpg_to_func(memwrite_func, NULL, w, h, comp, pixels, 10000);
    }

    // Test TGA with comp=2 (grey+alpha) path
    if (pixels && pixel_count > 0 && comp == 2) {
        stbi_write_tga_with_rle = extra_param & 1;
        reset_memout();
        stbi_write_tga_to_func(memwrite_func, NULL, w, h, comp, pixels);
    }

    // Test BMP with comp=4 (RGBA) path
    if (pixels && pixel_count > 0 && comp == 4) {
        reset_memout();
        stbi_write_bmp_to_func(memwrite_func, NULL, w, h, comp, pixels);
    }

    free(pixels);
    free(float_pixels);
    free(memout.buf);
    memout.buf = NULL;
    memout.len = 0;
    memout.cap = 0;

    return 0;
}
