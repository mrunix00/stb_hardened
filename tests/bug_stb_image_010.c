#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#include "stb_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    printf("BUG-stb_image-010: Testing GIF from_memory NULL flip...\n");

    // Enable vertical flip
    stbi_set_flip_vertically_on_load(1);

    // Initialize stack variables to force a positive *z value
    // so the flip loop enters and crashes on NULL result
    int x = 10, y = 10, z = 5, comp = 4;

    // Pass a non-GIF file to stbi_load_gif_from_memory
    // stbi__load_gif_main returns NULL without touching *z
    // Before fix: stbi__vertical_flip_slices dereferences NULL
    unsigned char not_gif[] = { 0, 0, 0, 0 };
    int *delays = NULL;
    stbi_uc *img = stbi_load_gif_from_memory(not_gif, sizeof(not_gif), &delays, &x, &y, &z, &comp, 4);
    if (img != NULL) {
        printf("FAIL: expected NULL but got image\n");
        stbi_image_free(img);
        if (delays) free(delays);
        return 1;
    }
    if (delays) free(delays);
    printf("Failed as expected (no NULL deref): %s\n", stbi_failure_reason());

    stbi_set_flip_vertically_on_load(0);
    printf("ALL PASSED\n");
    return 0;
}
