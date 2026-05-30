#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#include "stb_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    unsigned char data[] = {
        0x47,0x49,0x46,0x38,0x39,0x61,0xbd,0x00,0xdf,0x79,0xa9,0x97,
        0x66,0x4f,0x4e,0x4c,0xda,0x21,0xf9,0x04,0x09,0x0a,0x00,0x1f,
        0x00,0x2c
    };
    size_t size = sizeof(data);

    stbi_set_flip_vertically_on_load(1);
    printf("BUG-stb_image-006: Verifying bytes_per_pixel fix in vertical flip...\n");

    int x, y, z, channels;
    unsigned char *img = stbi_load_gif_from_memory(data, (int)size, NULL, &x, &y, &z, &channels, 2);
    if (img) {
        printf("OK: loaded %d frames (%dx%d, comp=%d), img=%p\n", z, x, y, channels, img);
        stbi_image_free(img);
        return 0;
    } else {
        printf("Failed: %s\n", stbi_failure_reason());
        return 0;
    }
}
