// Validation test for BUG-STB_HEXWAVE-007
// Out-of-bounds array access in segment-finding loop when
// hex->t >= 1.0 (all segment endpoint times are in [0,1],
// so the loop exits with j=8, then vert[9] is accessed).
//
// Expected result against UNPATCHED library:
//   UBSan/ASan OOB access at stb_hexwave.h:495 → exit 1 (crash)
// Expected result against PATCHED library: clean exit 0

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

    // Allocate init buffer
    size_t buf_size = (size_t)16 * width * (oversample + 1);
    float *buf = (float *)malloc(buf_size);
    if (!buf) { fprintf(stderr, "FAIL: init malloc\n"); return 1; }
    memset(buf, 0, buf_size);
    hexwave_init(width, oversample, buf);

    // Use non-collapsed shape so no infinite loops after wrap
    HexWave osc;
    hexwave_create(&osc, 1, 0.5f, 0.5f, 0.0f);

    // Set t exactly at 1.0 — all vert[] t values are <= 1.0,
    // so the segment-finding loop exits with j=8, which causes
    // vert[9] to be accessed (OOB without the wrap fix).
    osc.t = 1.0f;

    float out[256];
    memset(out, 0, sizeof(out));

    printf("Calling hexwave_generate_samples with t=%.1f...\n", osc.t);
    printf("BUG-007: segment-finding loop should exit j=8, OOB at vert[9] without fix.\n");

    hexwave_generate_samples(out, 256, &osc, 0.1f);

    printf("Survived! No OOB detected. t=%.6f\n", osc.t);
    printf("Output[0..3]=%f %f %f %f\n", out[0], out[1], out[2], out[3]);

    hexwave_shutdown(buf);
    free(buf);
    return 0;
}
