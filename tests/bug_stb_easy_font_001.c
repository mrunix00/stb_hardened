#define STB_EASY_FONT_IMPLEMENTATION
#include "stb_easy_font.h"
#include <stdio.h>
#include <string.h>

int main() {
    char buffer[10000];
    char text[4];

    // Test char < 32
    text[0] = 31; text[1] = 0;
    printf("Testing char 31...\n");
    stb_easy_font_width(text);
    stb_easy_font_print(0, 0, text, NULL, buffer, sizeof(buffer));

    // Test char > 126
    text[0] = 127; text[1] = 0;
    printf("Testing char 127...\n");
    stb_easy_font_width(text);
    stb_easy_font_print(0, 0, text, NULL, buffer, sizeof(buffer));

    // Test negative char (if signed)
    text[0] = (char)255; text[1] = 0;
    printf("Testing char 255...\n");
    stb_easy_font_width(text);
    stb_easy_font_print(0, 0, text, NULL, buffer, sizeof(buffer));

    printf("All tests finished (if no crash).\n");
    return 0;
}
