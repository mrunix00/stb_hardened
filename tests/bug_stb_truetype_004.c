#include <stdlib.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "../stb_truetype.h"

int main(void)
{
    static const unsigned char poc[] = {
        0x77, 0x4f, 0x46, 0x32, 0x80, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x0e, 0x54, 0x00, 0x0c, 0x00, 0x00
    };
    unsigned char *fontBuffer = (unsigned char *)malloc(sizeof(poc));
    stbtt_fontinfo info;
    if (!fontBuffer)
        return 2;
    memcpy(fontBuffer, poc, sizeof(poc));
    (void)stbtt_InitFont(&info, fontBuffer, 0);
    free(fontBuffer);
    return 0;
}
