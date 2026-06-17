#define STB_EASY_FONT_IMPLEMENTATION
#include "stb_easy_font.h"
#include <stdio.h>

int main() {
    char buffer[10000];
    int result;

    printf("Testing BUG-003: NULL text dereference...\n");

    /* stb_easy_font_print with text=NULL should crash without a NULL guard.
     * vbuf_size=0 to ensure we never write to vertex_buffer (that's BUG-004). */
    result = stb_easy_font_print(0, 0, NULL, NULL, buffer, 0);
    printf("  stb_easy_font_print returned %d\n", result);

    printf("BUG-003: PASS (no crash from NULL text)\n");
    return 0;
}
