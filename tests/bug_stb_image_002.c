#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void test_recursion() {
    stbi__gif g;
    memset(&g, 0, sizeof(g));

    g.w = 1; g.h = 1;
    g.out = malloc(4);
    g.history = malloc(1);
    g.max_y = 1;
    g.max_x = 4;
    g.line_size = 4;
    g.color_table = malloc(256 * 4);
    memset(g.color_table, 0, 256 * 4);

    for (int i = 1; i < 4096; i++) {
        g.codes[i].prefix = (stbi__int16)(i - 1);
        g.codes[i].suffix = 0;
    }
    g.codes[0].prefix = -1;
    g.codes[0].suffix = 0;

    printf("Calling stbi__out_gif_code with depth 4095 (BUG-stb_image-002)...\n");
    stbi__out_gif_code(&g, 4095);
    printf("Returned successfully from long chain\n");

    g.codes[0].prefix = 4095;
    printf("Calling stbi__out_gif_code with circular prefixes...\n");
    stbi__out_gif_code(&g, 4095);
    printf("Returned successfully from circle\n");

    free(g.out);
    free(g.history);
    free(g.color_table);
}

int main() {
    test_recursion();
    return 0;
}
