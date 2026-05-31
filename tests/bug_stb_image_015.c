// BUG-stb_image-015: PNG IHDR accepts bit_depth/color_type combinations
// that violate the PNG specification. Color types 2 (RGB), 4 (GA), 6 (RGBA)
// are restricted to depths 8 and 16, but stb_image also accepts 1, 2, 4.
// This causes the bit-unpacking code to misinterpret pixel boundaries.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_PNM
#include "stb_image.h"
#include <stdio.h>
#include <stdlib.h>

// PNG with color_type=2 (RGB) and bit_depth=4 (per spec, RGB requires depth 8 or 16)
static const unsigned char invalid_png[] = {
  0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
  0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
  0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02,
  0x04, 0x02, 0x00, 0x00, 0x00, 0x38, 0x24, 0x77,
  0x72, 0x00, 0x00, 0x00, 0x10, 0x49, 0x44, 0x41,
  0x54, 0x78, 0x9c, 0x63, 0xf8, 0xc0, 0xff, 0x81,
  0x81, 0xff, 0x03, 0x3f, 0x00, 0x0d, 0xbe, 0x02,
  0xfe, 0xdc, 0x15, 0xc2, 0xfc, 0x00, 0x00, 0x00,
  0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60,
  0x82,
};

int main(void) {
    int x, y, comp;
    unsigned char *img = stbi_load_from_memory(invalid_png, sizeof(invalid_png), &x, &y, &comp, 0);
    if (img) {
        printf("FAIL: Spec-violating PNG loaded successfully (%dx%d comp=%d)\n", x, y, comp);
        stbi_image_free(img);
        return 1;
    }
    printf("PASS: Spec-violating PNG correctly rejected: %s\n", stbi_failure_reason());
    return 0;
}
