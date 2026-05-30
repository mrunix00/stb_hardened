#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#include "stb_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    printf("BUG-stb_image-009: Testing PIC NULL deref in convert_format...\n");

    // PIC magic (4) + padding (84) + "PICT" (4) = 92 bytes
    unsigned char pic[128];
    memset(pic, 0, sizeof(pic));
    // Magic
    pic[0] = 0x53; pic[1] = 0x80; pic[2] = 0xF6; pic[3] = 0x34;
    // "PICT" at offset 88
    pic[88] = 'P'; pic[89] = 'I'; pic[90] = 'C'; pic[91] = 'T';
    // Width=1, Height=1 (small to minimize allocation)
    pic[92] = 0; pic[93] = 1;  // x = 1
    pic[94] = 0; pic[95] = 1;  // y = 1
    // ratio, fields, pad are already zero

    int x, y, comp = 0;
    // Use req_comp=3 so convert_format is reached (3 != 4 = img_n)
    // Inside convert_format, `data` is NULL -> NULL deref
    stbi_uc *img = stbi_load_from_memory(pic, sizeof(pic), &x, &y, &comp, 3);
    if (img != NULL) {
        printf("FAIL: expected NULL but got image %p\n", img);
        stbi_image_free(img);
        return 1;
    }
    printf("Failed as expected (req_comp=3): %s\n", stbi_failure_reason());

    printf("ALL PASSED\n");
    return 0;
}
