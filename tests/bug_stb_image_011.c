#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#include "stb_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    printf("BUG-stb_image-011: Testing convert_format16 overflow fix...\n");

    // 16-bit PGM (grayscale) -> req_comp=4 triggers stbi__convert_format16
    // Using stbi__malloc_mad4 instead of raw req_comp*x*y*2 to prevent overflow
    // PGM 16-bit values are big-endian; no byte-swapping on LE machines
    unsigned char pgm[] = {
        'P','5',' ','3',' ','3',' ','6','5','5','3','5','\n',
        0x00,0x00, 0x00,0x01, 0x00,0x02,
        0x00,0x03, 0x00,0x04, 0x00,0x05,
        0x00,0x06, 0x00,0x07, 0x00,0x08
    };
    int x, y, comp;
    unsigned short *img = (unsigned short *)stbi_load_16_from_memory(pgm, sizeof(pgm), &x, &y, &comp, 4);
    if (!img) {
        printf("FAIL: load returned NULL (%s)\n", stbi_failure_reason());
        return 1;
    }
    if (x != 3 || y != 3) {
        printf("FAIL: bad dims: %dx%d\n", x, y);
        stbi_image_free(img);
        return 1;
    }
    // comp returns original channel count (1 for grayscale), not requested (4)
    // pixel data should be RGBA: r=g=b=src, a=0xffff
    // Pixel values are in big-endian byte order (as stored in PGM file)
    if (img[0] != 0 || img[3] != 0xffff) {
        printf("FAIL: unexpected pixel0 values: %d %d %d %d\n", img[0], img[1], img[2], img[3]);
        stbi_image_free(img);
        return 1;
    }
    stbi_image_free(img);
    printf("ALL PASSED\n");
    return 0;
}
