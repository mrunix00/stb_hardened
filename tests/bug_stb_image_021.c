/*
 * Validation test for BUG-stb_image-021.
 *
 * The fuzzer found that an 18-byte TGA with a declared 65281x511 RLE-grayscale
 * image but only 1 byte of pixel data causes the decoder to run ~33 million
 * useless RLE loop iterations, taking 1.5+ seconds under ASan+UBSan and
 * 15+ seconds under libFuzzer with -timeout=10.
 *
 * This test reproduces the input, arms a SIGALRM with a 2-second budget, and
 * aborts with a non-zero exit if the load doesn't complete in time. After the
 * fix, the decoder bails out on EOF and the test returns 0.
 */
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static void on_alarm(int sig)
{
    (void)sig;
    /* Use _exit so ASan doesn't intercept the abort and report a different
     * (and misleading) crash summary. */
    static const char msg[] =
        "FAIL: BUG-stb_image-021 still present: TGA load did not complete "
        "within the time budget.\n";
    ssize_t w = write(2, msg, sizeof(msg) - 1);
    (void)w;
    _exit(1);
}

int main(void)
{
    /* The original fuzzer input that triggered the timeout, stripped of the
     * 1-byte mode selector. Width=0xff01 (65281), height=0x01ff (511),
     * bits_per_pixel=15, RLE-grayscale. */
    static const unsigned char tga_payload[17] = {
        0x00, 0x00, 0x0b,                       /* id, colormap, type (RLE gray) */
        0xe4, 0x04, 0x01, 0x01, 0x01,           /* colormap spec */
        0x00, 0x00, 0x00, 0xff,                 /* x_origin, y_origin */
        0x01, 0xff, 0xff, 0x01,                 /* width, height */
        0x0f                                    /* bits_per_pixel */
    };

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_alarm;
    sigaction(SIGALRM, &sa, NULL);
    alarm(2);

    int x = 0, y = 0, channels = 0;
    clock_t start = clock();
    unsigned char *img = stbi_load_from_memory(tga_payload, sizeof(tga_payload),
                                               &x, &y, &channels, 0);
    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;

    /* Cancel the alarm — we completed in time. */
    alarm(0);

    if (img) stbi_image_free(img);
    printf("TGA load returned %dx%d channels=%d in %.2fs\n",
           x, y, channels, elapsed);

    if (elapsed > 1.5) {
        printf("FAIL: BUG-stb_image-021 still present (load took %.2fs)\n",
               elapsed);
        return 1;
    }
    printf("PASS: BUG-stb_image-021 fixed (load completed in %.2fs)\n", elapsed);
    return 0;
}
