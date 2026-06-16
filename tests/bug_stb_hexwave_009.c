// Validation test for BUG-STB_HEXWAVE-009
// Infinite loop when freq is NaN or FLT_MAX.
//
// When freq = NaN: dt = NaN, the inner loop's `t < vert[j+1].t`
// is always false (NaN comparison), so transitions fire without
// progressing, and `t -= 1.0` at the wrap keeps t at NaN. Loop forever.
//
// When freq = FLT_MAX: dt = 3.4e38, t becomes FLT_MAX in one inner-loop
// iteration. The wrap `t -= 1.0` doesn't change t (float32 precision
// limit), so t never decreases. Loop forever.
//
// Detection: run with a timeout; if the call completes, the bug is fixed.
// Against the unpatched library, this test hangs indefinitely.

#define STB_HEXWAVE_IMPLEMENTATION
#include "stb_hexwave.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

int test_nan_freq()
{
    int width = 32;
    int oversample = 16;

    size_t buf_size = (size_t)16 * width * (oversample + 1);
    float *buf = (float *)malloc(buf_size);
    if (!buf) { fprintf(stderr, "FAIL: init malloc\n"); return 1; }
    memset(buf, 0, buf_size);
    hexwave_init(width, oversample, buf);

    HexWave osc;
    hexwave_create(&osc, 1, 0.0f, 0.0f, 0.0f);

    float out[256];
    memset(out, 0, sizeof(out));

    printf("  freq=NaN... ");
    fflush(stdout);
    hexwave_generate_samples(out, 256, &osc, NAN);
    printf("completed.\n");

    hexwave_shutdown(buf);
    free(buf);
    return 0;
}

int test_inf_freq()
{
    int width = 32;
    int oversample = 16;

    size_t buf_size = (size_t)16 * width * (oversample + 1);
    float *buf = (float *)malloc(buf_size);
    if (!buf) { fprintf(stderr, "FAIL: init malloc\n"); return 1; }
    memset(buf, 0, buf_size);
    hexwave_init(width, oversample, buf);

    HexWave osc;
    hexwave_create(&osc, 1, 0.0f, 0.0f, 0.0f);

    float out[256];
    memset(out, 0, sizeof(out));

    printf("  freq=INFINITY... ");
    fflush(stdout);
    hexwave_generate_samples(out, 256, &osc, INFINITY);
    printf("completed.\n");

    hexwave_shutdown(buf);
    free(buf);
    return 0;
}

int main()
{
    int fail = 0;

    printf("Testing BUG-STB_HEXWAVE-009 (infinite loop with extreme freq):\n");

    if (test_nan_freq()) { fprintf(stderr, "FAIL: NaN test\n"); fail = 1; }
    if (test_inf_freq()) { fprintf(stderr, "FAIL: INFINITY test\n"); fail = 1; }

    if (fail)
        return 1;

    printf("All BUG-009 tests passed.\n");
    return 0;
}
