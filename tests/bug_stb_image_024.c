#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <stdio.h>
#include <string.h>

/* TGA with very large declared width/height — should be rejected, not
 * allocated. Width=2863, height=65508 → pcount ≈ 187M → tga_data ≈ 562MB.
 * The decoder used to allocate this much, run the RLE loop (which our
 * prior fix makes bounded), and then iterate the post-processing loop
 * O(pcount) — also a DoS. */

int main(void) {
    static const unsigned char tga[] = {
        0x00, 0x00, 0x0b,                       /* ID, colormap, type (RLE gray) */
        0xe4, 0x04, 0x0c, 0x01, 0x00,           /* colormap spec */
        0x00, 0x07, 0x01, 0x01,                 /* x_origin, y_origin */
        0x2f, 0x0b, 0xe4, 0xff,                 /* width=2863, height=65508 */
        0x20,                                   /* bits_per_pixel */
        0x5a, 0x47, 0x7c, 0x00, 0x00, 0x01, 0x00, 0x00,
        0x00, 0x01, 0x01, 0xff, 0xff, 0xff, 0x0b,
    };

    int x=0, y=0, channels=0;
    const char *reason = NULL;
    unsigned char *img = stbi_load_from_memory(
        tga, sizeof(tga), &x, &y, &channels, 0);
    reason = stbi_failure_reason();

    fprintf(stderr, "img=%p reason=%s\n", (void*)img, reason ? reason : "(null)");

    /* The library must NOT allocate ~562MB just to handle a tiny input.
     * The expected behaviour: rejection with "too large" before any
     * large allocation. */
    if (img != NULL) {
        fprintf(stderr, "FAIL: large TGA was accepted\n");
        stbi_image_free(img);
        return 1;
    }
    if (reason == NULL || strcmp(reason, "too large") != 0) {
        fprintf(stderr, "FAIL: expected reason=\"too large\", got \"%s\"\n",
                reason ? reason : "(null)");
        return 1;
    }
    return 0;
}
