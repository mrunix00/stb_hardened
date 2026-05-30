#define STB_EASY_FONT_IMPLEMENTATION
#include "stb_easy_font.h"
#include <stdio.h>
#include <limits.h>

// Since stb_easy_font_draw_segs is static, we can call it here because we included the implementation.
int main() {
    unsigned char segs[1] = { 1 | (0 << 3) | (0 << 4) }; // len=1, dx=0, y=0
    stb_easy_font_color c = {255,255,255,255};
    char *vbuf = (char *)0x1000; // Fake address
    int vbuf_size = INT_MAX;
    int offset = INT_MAX - 32;

    printf("Testing overflow in stb_easy_font_draw_segs...\n");
    // This should trigger UBSan if it overflows
    stb_easy_font_draw_segs(0, 0, segs, 1, 0, c, vbuf, vbuf_size, offset);

    printf("Test finished.\n");
    return 0;
}
