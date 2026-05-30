#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#include "stb_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    unsigned char data[] = {
        0x47,0x49,0x46,0x38,0x39,0x61,0x00,0x00,0x00,0x00,0xf8,0x0a,0xdc,
        0x04,0xfc,0x00,0x46,0x00,0x00,0x2c,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x46,0x00,0x00,0x2c,0x00,0x00
    };
    size_t size = sizeof(data);

    printf("BUG-stb_image-008: Testing double-free on realloc with size 0...\n");

    int x, y, z, channels;
    int *delays = NULL;
    unsigned char *img = stbi_load_gif_from_memory(data, (int)size, &delays, &x, &y, &z, &channels, 4);
    if (img) {
        printf("ERROR: Load succeeded (img=%p)\n", img);
        stbi_image_free(img);
        if (delays) free(delays);
        return 1;
    } else {
        printf("Failed as expected: %s\n", stbi_failure_reason());
        if (delays) free(delays);
    }
    return 0;
}
