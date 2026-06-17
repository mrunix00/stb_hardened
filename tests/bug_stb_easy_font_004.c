#define STB_EASY_FONT_IMPLEMENTATION
#include "stb_easy_font.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    char text[] = "Hello";

    printf("Testing BUG-004: NULL vertex_buffer write...\n");

    /* With vertex_buffer=NULL and vbuf_size >= 64, draw_segs should write to
     * NULL + offset, causing a NULL-pointer write that ASan detects. */
    stb_easy_font_print(0, 0, text, NULL, NULL, 64);

    /* If we survive the call (either because the fix is in place or because
     * ASan is not active), check that we can still do useful work. */
    printf("BUG-004: PASS (no crash or null write detected)\n");
    return 0;
}
