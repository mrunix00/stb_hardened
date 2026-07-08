#define STB_HEXWAVE_IMPLEMENTATION
#include "stb_hexwave.h"
#include <stdio.h>

/* BUG-STB-HEXWAVE-010: Use-After-Free after hexwave_shutdown.
 *
 * hexwave_shutdown() frees hexblep.blep / hexblep.blamp but leaves the
 * process-global hexblep state dangling (width still non-zero, pointers
 * still pointing at the freed heap). Any later hexwave_generate_samples()
 * reads the freed tables -> heap-use-after-free.
 *
 * Expected (bug present, unpatched): ASan aborts -> non-zero exit.
 * Expected (fixed): shutdown clears the state, generate runs in the safe
 * width==0 degenerate mode, no fault -> exit 0.
 */
int main(void)
{
    hexwave_init(32, 16, NULL);
    hexwave_shutdown(NULL);          /* frees blep/blamp; state left dangling */

    HexWave osc;
    hexwave_create(&osc, 1, 0.5f, 1.0f, 0.0f);
    float out[256];
    hexwave_generate_samples(out, 200, &osc, 0.3f);  /* UAF read of freed blep/blamp */

    printf("BUG-STB-HEXWAVE-010: generate-after-shutdown did not fault\n");
    return 0;
}
