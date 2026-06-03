#define STB_HEXWAVE_IMPLEMENTATION
#include "stb_hexwave.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main()
{
    // BUG-STB_HEXWAVE-001: memset(output,0) erases the frequency-change BLAMP
    // fixup. Compare two runs: same-freq (no BLAMP) vs diff-freq (BLAMP expected).
    // In the buggy version, output[0..halfw-1] is identical in both cases because
    // BLAMP is zeroed by memset. In the fixed version, they differ.
    int width = 32;
    int oversample = 16;
    size_t buf_size = (size_t)16 * width * (oversample + 1);
    float *buf = (float *)malloc(buf_size);
    if (!buf) { fprintf(stderr, "FAIL: malloc\n"); return 1; }
    memset(buf, 0, buf_size);
    hexwave_init(width, oversample, buf);

    int halfw = width / 2;
    HexWave osc_a, osc_b;
    float out_a[256], out_b[256];

    // Run A: same frequency (no BLAMP expected)
    hexwave_create(&osc_a, 1, 0.5f, 0.0f, 0.0f); // triangle
    hexwave_generate_samples(out_a, 128, &osc_a, 0.1f); // establish state
    memset(out_a, 0, sizeof(out_a));
    hexwave_generate_samples(out_a, 128, &osc_a, 0.1f); // same freq, no BLAMP

    // Run B: different frequency (BLAMP expected)
    hexwave_create(&osc_b, 1, 0.5f, 0.0f, 0.0f);
    hexwave_generate_samples(out_b, 128, &osc_b, 0.1f); // establish state
    memset(out_b, 0, sizeof(out_b));
    hexwave_generate_samples(out_b, 128, &osc_b, 0.2f); // freq change, BLAMP

    int diff_count = 0;
    for (int i = 0; i < halfw; i++) {
        float d = out_a[i] - out_b[i];
        if (d < 0) d = -d;
        if (d > 1e-4f) diff_count++;
    }

    if (diff_count > 0) {
        printf("BLAMP fixup detected (%d/%d samples differ > 1e-4) — fix working\n",
               diff_count, halfw);
        return 0;
    } else {
        printf("No BLAMP fixup found — bug present (memset erased it), or no diff req\n");
        return 1;
    }
}
