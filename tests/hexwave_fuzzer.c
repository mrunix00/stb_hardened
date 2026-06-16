// stb_hexwave.h fuzzer
// Layout: 40 bytes of HexWave state + freq, then rest is dummy output padding
//
// The fuzzer exercises hexwave_generate_samples with attacker-controlled
// oscillator state and freq to find crashes, OOB accesses, and hangs.

#ifdef __cplusplus
extern "C" {
#endif

#define STB_HEXWAVE_IMPLEMENTATION
#include "stb_hexwave.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Maximum number of samples per call — keep moderate to avoid OOM
#define MAX_SAMPLES 4096

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    // Need at least 40 bytes for the HexWave struct layout
    //   float t            (4 bytes)
    //   float prev_dt      (4 bytes)
    //   int   reflect      (4 bytes)
    //   float peak_time    (4 bytes)
    //   float half_height  (4 bytes)
    //   float zero_wait    (4 bytes)
    //   int   reflect_p    (4 bytes)  -- pending
    //   float peak_time_p  (4 bytes)
    //   float half_height_p(4 bytes)
    //   float zero_wait_p  (4 bytes)
    //   float freq         (4 bytes)  -- freq parameter
    if (size < 44) return 0;

    // Initialize the library with a fixed safe config.
    // Since we're fuzzing generate_samples, not init, we keep
    // parameters reasonable to avoid noise from init failures.
    static int initialized = 0;
    if (!initialized) {
        // user_buffer mode to avoid malloc in the hot path
        // allocate enough: 16 * width * (oversample+1) bytes
        size_t buf_sz = (size_t)16 * 32 * (16 + 1) * sizeof(float);
        float *buf = (float *)malloc(buf_sz);
        if (!buf) return 0;
        memset(buf, 0, buf_sz);
        hexwave_init(32, 16, buf);
        initialized = 1;
    }

    // Decode state from fuzz data
    float t;
    float prev_dt;
    int reflect, reflect_p;
    float peak_time, half_height, zero_wait;
    float peak_time_p, half_height_p, zero_wait_p;
    float freq;
    int num_samples;

    memcpy(&t,              data + 0,  4);
    memcpy(&prev_dt,        data + 4,  4);
    memcpy(&reflect,        data + 8,  4);
    memcpy(&peak_time,      data + 12, 4);
    memcpy(&half_height,    data + 16, 4);
    memcpy(&zero_wait,      data + 20, 4);
    memcpy(&reflect_p,      data + 24, 4);
    memcpy(&peak_time_p,    data + 28, 4);
    memcpy(&half_height_p,  data + 32, 4);
    memcpy(&zero_wait_p,    data + 36, 4);
    memcpy(&freq,           data + 40, 4);

    // num_samples derived from remaining data size
    num_samples = (int)(size - 44);
    if (num_samples > MAX_SAMPLES) num_samples = MAX_SAMPLES;
    if (num_samples < 1) num_samples = 1;

    // Build oscillator with fuzzed parameters
    HexWave osc;
    memset(&osc, 0, sizeof(osc));
    osc.t = t;
    osc.prev_dt = prev_dt;
    osc.current.reflect     = reflect;
    osc.current.peak_time   = peak_time;
    osc.current.half_height = half_height;
    osc.current.zero_wait   = zero_wait;
    osc.pending.reflect     = reflect_p;
    osc.pending.peak_time   = peak_time_p;
    osc.pending.half_height = half_height_p;
    osc.pending.zero_wait   = zero_wait_p;
    osc.have_pending = 1;  // always try to apply a pending change

    // Output buffer — use the remaining data as initial content
    float *output = (float *)malloc(sizeof(float) * (size_t)num_samples);
    if (!output) return 0;
    
    // Copy fuzz data tail into output for more varied starting state
    size_t copy_bytes = sizeof(float) * (size_t)num_samples;
    if (copy_bytes > size - 44)
        copy_bytes = size - 44;
    memset(output, 0, sizeof(float) * (size_t)num_samples);
    memcpy(output, data + 44, copy_bytes);

    // Call the target function
    hexwave_generate_samples(output, num_samples, &osc, freq);

    free(output);
    return 0;
}

#ifdef __cplusplus
}
#endif
