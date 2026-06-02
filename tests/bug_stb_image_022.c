/*
 * Validation test for BUG-stb_image-022.
 *
 * The fuzzer found that a 44-byte GIF89a with a declared image size of
 * 18688x14406 (via overlapping width/height bytes) triggers three
 * allocations totalling ~1.3GB in stbi__gif_load_next before any image
 * data is decoded. With a 2GB RSS limit (libFuzzer default), the process
 * is OOM-killed.
 *
 * This test verifies the fix by checking that the loader rejects the
 * over-sized image early (returning "too large") before any allocation
 * of that size. On the unpatched library the load either OOMs the
 * process (which terminates it with a non-zero exit, considered a test
 * failure) or returns an all-zero 1.3GB buffer. After the fix, the
 * loader returns NULL with failure reason "too large".
 */
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void on_alarm(int sig)
{
    (void)sig;
    static const char msg[] =
        "FAIL: BUG-stb_image-022 still present: GIF load did not complete "
        "within the time budget.\n";
    ssize_t w = write(2, msg, sizeof(msg) - 1);
    (void)w;
    _exit(1);
}

int main(void)
{
    /* 44-byte GIF89a found by the fuzzer. The width/height bytes overlap
     * with a second "IF89a" magic, yielding 18688x14406.
     *
     *   47 49 46 38 39 61  -- GIF89a
     *   00 49              -- width = 0x4900 = 18688
     *   46 38              -- height = 0x3846 = 14406
     *   ... rest is malformed body. */
    static const unsigned char gif_payload[44] = {
        0x47, 0x49, 0x46, 0x38, 0x39, 0x61,  /* GIF89a */
        0x00, 0x49, 0x46, 0x38, 0x39, 0x61,  /* width=18688, "IF89a" */
        0x00, 0x00, 0x80,                    /* GCT flag, bg=0, ratio=0 */
        0x00, 0x00,                          /* bg color */
        0x2c, 0xff, 0xff, 0x00, 0x00, 0x00,  /* image descriptor (placeholder) */
        0x2c, 0x01, 0x00, 0x10, 0x47,        /* LZW min code + 4 bytes body */
        0x00, 0x80, 0x00, 0x00, 0x2c, 0xff, 0xff, 0x00, 0x00, 0x00,
        0x2c, 0x01, 0x00, 0x10, 0x47
    };

    /* Arm a generous alarm to catch any other infinite loop / hang. */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_alarm;
    sigaction(SIGALRM, &sa, NULL);
    alarm(10);

    int x = 0, y = 0, channels = 0;
    unsigned char *img = stbi_load_from_memory(gif_payload, sizeof(gif_payload),
                                               &x, &y, &channels, 0);
    alarm(0);

    if (img) stbi_image_free(img);

    const char *reason = stbi_failure_reason();
    printf("GIF load returned %dx%d channels=%d reason=\"%s\"\n",
           x, y, channels, reason ? reason : "(none)");

    /* After the fix the image should be rejected with "too large" or
     * similar before any large allocation.  On the unpatched library,
     * stbi_load_from_memory will:
     *   1. Either succeed in allocating ~1.3GB and then either succeed
     *      in decoding the garbage (returning a 1.3GB zero buffer), or
     *      fail later in the LZW decoder with "unknown code" / similar.
     *      In either case, ~1.3GB has been malloc'd, which is the bug.
     *   2. Or, in OOM-constrained runs, fail with "outofmem" or be
     *      killed by the OOM killer.
     *   Either way, returning "too large" early is the expected fix. */
    if (reason && strcmp(reason, "too large") == 0) {
        printf("PASS: BUG-stb_image-022 fixed (rejected with \"too large\")\n");
        return 0;
    }
    /* Reaching here means the bug is still present: the loader did not
     * reject the image up front.  Whether the load returned a buffer
     * or failed downstream, the ~1.3GB allocation already happened. */
    printf("FAIL: BUG-stb_image-022 still present (load returned "
           "%dx%d, %d bytes allocated; reason=\"%s\")\n",
           x, y, x * y * 4, reason ? reason : "(none)");
    return 1;
}
