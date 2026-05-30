#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#include "stb_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    // 23170 x 23170
    unsigned char gif_header[] = {
        'G', 'I', 'F', '8', '9', 'a',
        0x82, 0x5a, 0x82, 0x5a, // 23170 x 23170
        0x80, 0x00, 0x00,
        0xff, 0xff, 0xff, 0x00, 0x00, 0x00 // GCT
    };

    unsigned char gif_frame[] = {
        0x2c, 0x00, 0x00, 0x00, 0x00, 0x82, 0x5a, 0x82, 0x5a, 0x00,
        0x02, 0x01, 0x44, 0x00
    };

    int layers = 2;
    size_t gif_size = sizeof(gif_header) + layers * sizeof(gif_frame) + 1;
    unsigned char *gif = malloc(gif_size);
    memcpy(gif, gif_header, sizeof(gif_header));
    for (int i = 0; i < layers; i++) {
        memcpy(gif + sizeof(gif_header) + i * sizeof(gif_frame), gif_frame, sizeof(gif_frame));
    }
    gif[gif_size - 1] = 0x3b; // trailer

    int x, y, z, comp;
    int *delays = NULL;

    printf("Starting BUG-stb_image-001 validation with %d layers of size 23170x23170...\n", layers);
    // Passing &delays directly.
    unsigned char *data = stbi_load_gif_from_memory(gif, (int)gif_size, &delays, &x, &y, &z, &comp, 0);

    if (data) {
        printf("Success: loaded %d frames\n", z);
        stbi_image_free(data);
        if (delays) free(delays);
    } else {
        printf("Failed as expected: %s\n", stbi_failure_reason());
        // delays is already freed by stbi__load_gif_main_outofmem if it returned NULL
    }

    free(gif);
    return 0;
}
