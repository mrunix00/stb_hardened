#define STB_HEXWAVE_IMPLEMENTATION
#include "stb_hexwave.h"
#include <stdio.h>
#include <math.h>

/* BUG-STB-HEXWAVE-012: half_height is accepted unvalidated.
 *
 * This is a NEGATIVE / security test. The concern would be: does passing a
 * non-finite or extreme half_height to hexwave_create/hexwave_change cause a
 * memory-safety fault (OOB read/write, infinite loop, NaN-driven UB) in
 * hexwave_generate_samples?  Segment TIMES (vert[].t) depend only on the
 * clamped zero_wait/peak_time/reflect, so the synthesis loops stay bounded and
 * never read/write out of bounds. Non-finite half_height only propagates into
 * the OUTPUT as NaN/Inf (a documented contract limitation, since the header
 * explicitly states "half_height is not clamped" to allow morphs with |h|>1).
 *
 * Expected: under ASan+UBSan, generate_samples must NOT fault. The test exits 0
 * (no memory-safety bug). Output containing NaN/Inf is expected and printed,
 * not treated as a failure.
 */
static int any_nonfinite(const float *a, int n)
{
    for (int i = 0; i < n; ++i)
        if (!isfinite(a[i])) return 1;
    return 0;
}

int main(void)
{
    float halves[] = { (float)NAN, (float)INFINITY, -INFINITY, 1e30f, -1e30f, 2.0f, -1.0f };
    hexwave_init(32, 16, NULL);
    for (size_t h = 0; h < sizeof(halves)/sizeof(halves[0]); ++h) {
        HexWave osc;
        hexwave_create(&osc, 1, 0.5f, halves[h], 0.0f);
        float out[512];
        for (int b = 0; b < 3; ++b)
            hexwave_generate_samples(out, 512, &osc, 0.3f);
        /* change mid-stream with another extreme half_height */
        hexwave_change(&osc, 0, 0.0f, halves[h], 0.5f);
        hexwave_generate_samples(out, 512, &osc, 0.25f);
        printf("half_height=%g -> output non-finite: %d\n", halves[h], any_nonfinite(out, 512));
    }
    hexwave_shutdown(NULL);
    printf("BUG-STB-HEXWAVE-012: no memory-safety fault observed with extreme half_height\n");
    return 0;
}
