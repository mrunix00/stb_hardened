#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <stdio.h>
#include <string.h>

/* BMP with offset=0, hsz=40, bpp=8 => psize = (0-14-40)>>2 = -14.
 * Negative psize bypasses both palette validation checks (psize==0 and
 * psize>256), the palette is not read, and stack-allocated pal[256][4]
 * remains uninitialized. Pixel values index directly into this
 * uninitialized stack memory, leaking its contents into the output.
 *
 * With the bug present: the decoder accepts the file (returns non-NULL)
 * and pixel data contains uninitialized stack contents.
 * After the fix: the decoder rejects the malformed BMP (returns NULL). */

int main(void) {
    static const unsigned char bmp[] = {
        0x42,0x4D,             /* 'BM' */
        0x46,0x00,0x00,0x00,   /* filesize (70) */
        0x00,0x00,             /* reserved */
        0x00,0x00,             /* reserved */
        0x00,0x00,0x00,0x00,   /* offset = 0 => negative psize */
        0x28,0x00,0x00,0x00,   /* hsz = 40 (BITMAPINFOHEADER) */
        0x02,0x00,0x00,0x00,   /* width = 2 */
        0x02,0x00,0x00,0x00,   /* height = 2 */
        0x01,0x00,             /* planes = 1 */
        0x08,0x00,             /* bpp = 8 (indexed) */
        0x00,0x00,0x00,0x00,   /* compression = BI_RGB */
        0x00,0x00,0x00,0x00,   /* image size (ignored) */
        0x00,0x00,0x00,0x00,   /* xres (ignored) */
        0x00,0x00,0x00,0x00,   /* yres (ignored) */
        0x00,0x00,0x00,0x00,   /* colors used (ignored) */
        0x00,0x00,0x00,0x00,   /* colors important (ignored) */
        /* pixel data: 2 rows x 2 cols x 1 byte (index) */
        0x00,0x01,0x02,0x03,
    };

    int x=0, y=0, channels=0;
    unsigned char *img = stbi_load_from_memory(
        bmp, sizeof(bmp), &x, &y, &channels, 3);

    if (img != NULL) {
        /* Bug confirmed: decoder accepted the malformed BMP.
         * The pixel data contains uninitialized stack memory. */
        fprintf(stderr, "BUG-026 PRESENT: BMP with offset=0 was accepted "
                "(x=%d y=%d c=%d). "
                "Pixel data reads uninitialized stack palette.\n",
                x, y, channels);
        stbi_image_free(img);
        return 1; /* test fails — bug is present */
    }

    /* After the fix, the decoder rejects this input. */
    fprintf(stderr, "BUG-026 FIXED: BMP with offset=0 correctly rejected: %s\n",
            stbi_failure_reason() ? stbi_failure_reason() : "(null)");
    return 0;
}
