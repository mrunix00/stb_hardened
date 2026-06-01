#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <stdlib.h>
#include <math.h>
#include <signal.h>

static int caught_signal = 0;

static void handler(int sig) {
    caught_signal = 1;
}

static void discard_write(void *context, void *data, int size) {
    (void)context; (void)data; (void)size;
}

int main(void)
{
    struct sigaction sa, old;
    sa.sa_handler = handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGABRT, &sa, &old);
    sigaction(SIGSEGV, &sa, NULL);

    /* Test 1: infinity in first RGB channel triggers UB via inf*0 = NaN */
    {
        float buf[4] = { (float)INFINITY, 0.0f, 0.0f, 1.0f };
        stbi_write_hdr_to_func(discard_write, NULL, 1, 1, 3, buf);
    }

    /* Test 2: all-NaN RGB also triggers UB */
    {
        float buf[4] = { (float)NAN, (float)NAN, (float)NAN, 1.0f };
        stbi_write_hdr_to_func(discard_write, NULL, 1, 1, 3, buf);
    }

    sigaction(SIGABRT, &old, NULL);

    /* If we caught a signal from UBSan, the bug is present */
    if (caught_signal)
        return 1;

    return 0;
}
