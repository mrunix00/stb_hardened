/*
 * Validation test for BUG-stb_image-023.
 *
 * BUG-stb_image-023: stbi__getn (stb_image.h:1683) calls
 *   memcpy(buffer, s->img_buffer, n)
 * even when n == 0, and even when the pointer arguments could be NULL.
 * Glibc declares memcpy with __nonnull on both pointer parameters, so
 * passing a NULL pointer (even with a zero size) is undefined behaviour.
 *
 * This test uses stbi_load_from_callbacks so that the context's
 * img_buffer and img_buffer_end are both NULL (the initial state for
 * callback-backed contexts).  A TGA with width=0 makes the non-RLE
 * pixel path call stbi__getn(s, tga_row, 0), which under the callback
 * path reaches:
 *
 *   if (s->img_buffer+n <= s->img_buffer_end)  // NULL+0 <= NULL → true
 *       memcpy(tga_row, NULL, 0);              // UBSAN fires!
 *
 * To make UBSAN abort on the null-pointer-argument check we compile
 * with -fno-sanitize-recover=undefined.
 *
 * The fix is a one-line guard:  if (n) memcpy(...);
 */

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Custom io callback returning our crafted TGA. */
static const unsigned char *g_input = NULL;
static size_t g_input_pos = 0;
static size_t g_input_len = 0;

static int read_cb(void *user, char *data, int size)
{
    (void)user;
    size_t avail = g_input_len - g_input_pos;
    if (avail == 0) return 0;
    if ((size_t)size > avail) size = (int)avail;
    memcpy(data, g_input + g_input_pos, (size_t)size);
    g_input_pos += (size_t)size;
    return size;
}

static void skip_cb(void *user, int n)
{
    (void)user;
    if (n > 0) {
        size_t avail = g_input_len - g_input_pos;
        if ((size_t)n > avail) n = (int)avail;
        g_input_pos += (size_t)n;
    }
}

static int eof_cb(void *user)
{
    (void)user;
    return g_input_pos >= g_input_len ? 1 : 0;
}

int main(void)
{
    /*
     * TGA header: uncompressed grayscale (type 3), width=0, height=1,
     * bpp=8.  The 0 width passes all checks (0 <= STBI_MAX_DIMENSIONS,
     * mad3sizes_valid yields true).  The non-RLE pixel-read path calls
     * stbi__getn(s, tga_row, 0).
     */
    static const unsigned char tga[] = {
        0x00,       /* id length */
        0x00,       /* colour map type (none) */
        0x03,       /* image type: uncompressed grayscale */
        0x00, 0x00, /* colour map start */
        0x00, 0x00, /* colour map length */
        0x00,       /* colour map bits */
        0x00, 0x00, /* x origin */
        0x00, 0x00, /* y origin */
        0x00, 0x00, /* width  = 0 */
        0x01, 0x00, /* height = 1 */
        0x08,       /* bits per pixel = 8 */
        0x01,       /* descriptor */
    };

    stbi_io_callbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.read = read_cb;
    callbacks.skip = skip_cb;
    callbacks.eof = eof_cb;

    g_input = tga;
    g_input_pos = 0;
    g_input_len = sizeof(tga);

    int x = 0, y = 0, channels = 0;
    unsigned char *img = stbi_load_from_callbacks(
        &callbacks, NULL, &x, &y, &channels, 0);

    /*
     * Under UBSAN with -fno-sanitize-recover=undefined, the memcpy
     * with a NULL src pointer should abort before reaching here.
     *
     * If we reach here, either UBSAN didn't fire (e.g. the compiler
     * optimized the memcpy away or the nonnull check is not enforced
     * for this memcpy call pattern), or the code path changed.
     */
    if (img) {
        /* Unexpected: decoder returned data for a 0-width image. */
        stbi_image_free(img);
        fprintf(stderr, "FAIL: BUG-stb_image-023 not triggered — "
                        "decoder accepted 0-width TGA and returned non-NULL\n");
        return 1;
    }

    fprintf(stderr, "PASS: BUG-stb_image-023 path exercised "
                    "(no UBSAN abort on this platform)\n");
    return 0;
}
