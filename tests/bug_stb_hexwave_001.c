#define STB_HEXWAVE_IMPLEMENTATION
#include "stb_hexwave.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

int main()
{
    int width = 32;
    int oversample = 16;
    size_t buf_size = (size_t)16 * width * (oversample + 1);
    float *buf = (float *)malloc(buf_size);
    if (!buf) { fprintf(stderr, "FAIL: malloc\n"); return 1; }
    memset(buf, 0, buf_size);
    hexwave_init(width, oversample, buf);

    int halfw = width / 2;

    HexWave osc_a, osc_b;
    memset(&osc_a, 0, sizeof(osc_a));
    memset(&osc_b, 0, sizeof(osc_b));

    osc_a.current.reflect = 1;
    osc_a.current.peak_time = 0.5f;
    osc_a.current.half_height = 0.0f;
    osc_a.current.zero_wait = 0.0f;
    osc_a.have_pending = 0;
    osc_a.t = 0.6f;
    osc_a.prev_dt = 0.5f;

    osc_b = osc_a;
    osc_b.prev_dt = 0.1f;

    float output_a[256], output_b[256];
    memset(output_a, 0, sizeof(output_a));
    memset(output_b, 0, sizeof(output_b));

    float target_freq = 0.1f;
    hexwave_generate_samples(output_a, 128, &osc_a, target_freq);
    hexwave_generate_samples(output_b, 128, &osc_b, target_freq);

    int diff_count = 0;
    float max_diff = 0;
    int max_idx = -1;
    for (int i = 0; i < halfw; i++) {
        float d = output_a[i] - output_b[i];
        if (d < 0) d = -d;
        if (d > 1e-4f) diff_count++;
        if (d > max_diff) { max_diff = d; max_idx = i; }
    }

    printf("halfw=%d, diff_count=%d, max_diff=%.6f at idx %d\n",
           halfw, diff_count, max_diff, max_idx);
    printf("output_a[0..7]: ");
    for (int i = 0; i < 8; i++) printf("%.5f ", output_a[i]);
    printf("\noutput_b[0..7]: ");
    for (int i = 0; i < 8; i++) printf("%.5f ", output_b[i]);
    printf("\n");

    if (diff_count > 0) {
        printf("BLAMP fixup detected in output[0..halfw-1] — fix working\n");
        return 0;
    } else {
        printf("No BLAMP fixup found — bug present (memset erased it)\n");
        return 1;
    }
}
