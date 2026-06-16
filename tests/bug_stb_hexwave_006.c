// Validation test for BUG-STB_HEXWAVE-006
// Heap buffer overflow in frequency-change BLAMP fixup when
// num_samples < hexblep.width.
//
// The BLAMP fixup at stb_hexwave.h:456 writes hexblep.width floats
// to output[] via hex_blamp(). When num_samples < hexblep.width, the
// output buffer is smaller than hexblep.width, causing a write past
// the end.
//
// Detection: clear the entire region (output + guard) to zero,
// call hexwave_generate_samples, then check if guard bytes changed.

#define STB_HEXWAVE_IMPLEMENTATION
#include "stb_hexwave.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

    // Allocate: [output(4 floats)] + [guard(32 floats)]
    float *out = (float *)malloc(sizeof(float) * (4 + 32));
    if (!out) { fprintf(stderr, "FAIL: output malloc\n"); return 1; }

    // Clear ALL memory (output + guard) to zero
    memset(out, 0, sizeof(float) * (4 + 32));

    // Create oscillator with non-zero slope at the segment containing t=0
    // peak_time=0.5, half_height=0.5 creates a rising segment from 0 to 0.25
    // with slope = 4.0 for vertices near t=0
    HexWave osc;
    hexwave_create(&osc, 1, 0.5f, 0.5f, 0.0f);
    osc.prev_dt = 0.5f; // Ensure freq change detected

    // Call with num_samples=4 < hexblep.width=32
    // The freq-change BLAMP at line 456 writes hexblep.width floats to output[],
    // overflowing into the guard region
    hexwave_generate_samples(out, 4, &osc, 0.1f);

    // Check guard for corruption (must still be zero)
    int corrupted = 0;
    for (int i = 0; i < 32; ++i) {
        if (out[4 + i] != 0.0f) {
            fprintf(stderr,
                "FAIL: guard[%d] = %.6f (expected 0.0)\n",
                i, out[4 + i]);
            corrupted = 1;
        }
    }

    if (corrupted) {
        fprintf(stderr, "\nBUG-STB_HEXWAVE-006 VALIDATED: "
                        "freq-change BLAMP overflowed output buffer.\n");
        free(out);
        hexwave_shutdown(buf);
        free(buf);
        return 1; // Test fails (as expected — bug is present)
    }

    printf("PASS: No guard corruption detected.\n");

    free(out);
    hexwave_shutdown(buf);
    free(buf);
    return 0;
}
