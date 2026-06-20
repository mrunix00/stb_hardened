/*
 * bench_stb_hexwave.c — Performance benchmark for stb_hexwave.h
 *
 * Measures waveform generation throughput across buffer sizes, waveform
 * shapes, frequencies, and morphing patterns.
 *
 * Build:
 *   clang -O2 -DNDEBUG -I.. -I. tests/bench_stb_hexwave.c -lm -o build/bench_stb_hexwave
 *
 * Usage:
 *   ./build/bench_stb_hexwave [--quick] [--iters N] [--warmup N] [--help]
 */

#define STB_HEXWAVE_IMPLEMENTATION
#include "stb_hexwave.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ── Timing ──────────────────────────────────────────────────────────── */

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/* ── Options ─────────────────────────────────────────────────────────── */

static int g_quick       = 0;
static int g_fixed_iters = 0;
static int g_warmup      = 3;
static float g_sample_rate = 44100.0f;

/* ── Benchmark core ──────────────────────────────────────────────────── */

typedef struct {
    double min_ms;
    double mean_ms;
    double max_ms;
    int    iters;
    int    ok;
} bench_result;

typedef int (*bench_fn)(void *ctx);

static bench_result run_bench(bench_fn fn, void *ctx) {
    bench_result r = {0};
    int calib = 5;

    uint64_t t0 = now_ns();
    for (int i = 0; i < calib; i++)
        if (!fn(ctx)) { r.ok = 0; return r; }
    uint64_t t1 = now_ns();
    uint64_t per_run = (t1 - t0) / (uint64_t)calib;

    int iterations;
    if (g_fixed_iters > 0)
        iterations = g_fixed_iters;
    else {
        iterations = (per_run > 0) ? (int)(500000000ULL / per_run) : 500;
        if (iterations < 5) iterations = 5;
        if (iterations > 500) iterations = 500;
    }

    for (int i = 0; i < g_warmup; i++)
        if (!fn(ctx)) { r.ok = 0; return r; }

    t0 = now_ns();
    for (int i = 0; i < iterations; i++)
        fn(ctx);
    t1 = now_ns();
    r.mean_ms = (double)(t1 - t0) / iterations / 1e6;
    r.iters = iterations;

    int detail = iterations < 20 ? iterations : 20;
    double best = 1e18, worst = 0;
    for (int i = 0; i < detail; i++) {
        uint64_t a = now_ns();
        fn(ctx);
        uint64_t b = now_ns();
        double ms = (double)(b - a) / 1e6;
        if (ms < best) best = ms;
        if (ms > worst) worst = ms;
    }
    r.min_ms = best;
    r.max_ms = worst;
    r.ok = 1;
    return r;
}

/* ── Formatting helpers ──────────────────────────────────────────────── */

static void print_header(void) {
    printf("  %-28s %7s %9s %9s %9s %10s %8s\n",
           "Operation", "Samples", "Min(ms)", "Mean(ms)", "Max(ms)",
           "Msamp/s", "Iters");
    printf("  %-28s %7s %9s %9s %9s %10s %8s\n",
           "----------------------------", "-------",
           "---------", "---------", "---------",
           "----------", "--------");
}

static void print_result(const char *label, int nsamples, bench_result *r) {
    if (!r->ok) {
        printf("  %-28s %7d    FAILED\n", label, nsamples);
        return;
    }
    double msamp = (double)nsamples / (r->mean_ms * 1e3);
    printf("  %-28s %7d %9.3f %9.3f %9.3f %10.1f %8d\n",
           label, nsamples, r->min_ms, r->mean_ms, r->max_ms,
           msamp, r->iters);
}

static void print_header_simple(void) {
    printf("  %-28s %9s %9s %9s %10s %8s\n",
           "Operation", "Min(ms)", "Mean(ms)", "Max(ms)", "Msamp/s", "Iters");
    printf("  %-28s %9s %9s %9s %10s %8s\n",
           "----------------------------",
           "---------", "---------", "---------",
           "----------", "--------");
}

static void print_result_simple(const char *label, int nsamples, bench_result *r) {
    if (!r->ok) {
        printf("  %-28s    FAILED\n", label);
        return;
    }
    double msamp = (double)nsamples / (r->mean_ms * 1e3);
    printf("  %-28s %9.3f %9.3f %9.3f %10.1f %8d\n",
           label, r->min_ms, r->mean_ms, r->max_ms, msamp, r->iters);
}

static int failures = 0;

static void check_result(bench_result *r, const char *label) {
    if (!r->ok) { printf("  >> FAIL: %s\n", label); failures++; }
}

/* ── Benchmark context structs ───────────────────────────────────────── */

typedef struct {
    HexWave *osc;
    float *output;
    int nsamples;
    float freq;
} gen_ctx;

typedef struct {
    HexWave *osc;
    float *output;
    int nsamples;
    float freq;
    int reflect;
    float peak_time;
    float half_height;
    float zero_wait;
} morph_ctx;

/* ── Benchmark functions ─────────────────────────────────────────────── */

static int bench_generate(void *vctx) {
    gen_ctx *c = (gen_ctx *)vctx;
    hexwave_generate_samples(c->output, c->nsamples, c->osc, c->freq);
    return 1;
}

static int bench_change_then_generate(void *vctx) {
    morph_ctx *c = (morph_ctx *)vctx;
    hexwave_change(c->osc, c->reflect, c->peak_time, c->half_height, c->zero_wait);
    hexwave_generate_samples(c->output, c->nsamples, c->osc, c->freq);
    return 1;
}

/* ── Waveform shapes ─────────────────────────────────────────────────── */

typedef struct {
    const char *name;
    int reflect;
    float peak_time;
    float half_height;
    float zero_wait;
} waveform_t;

static waveform_t classic_waveforms[] = {
    { "Sawtooth",       1, 0.0f,  0.0f, 0.0f },
    { "Square",         1, 0.0f,  1.0f, 0.0f },
    { "Triangle",       1, 0.5f,  0.0f, 0.0f },
    { "AltSaw",         0, 0.0f,  0.0f, 0.0f },
    { "Stairs",         0, 0.0f,  1.0f, 0.5f },
    { "Saw8va",         1, 0.0f, -1.0f, 0.0f },
    { "Saw (neg)",      1, 1.0f,  0.0f, 0.0f },
    { "Triangle (alt)", 0, 0.5f,  0.0f, 0.0f },
};
static int n_classic = sizeof(classic_waveforms) / sizeof(classic_waveforms[0]);

/* ── Suites ──────────────────────────────────────────────────────────── */

static int bench_suite_buffer_size(HexWave *osc) {
    int sizes[] = { 256, 1024, 4096, 16384, 65536 };
    int n_sizes = g_quick ? 3 : 5;
    float freq = 440.0f / g_sample_rate; /* A4 */

    printf("\n── Suite 1: Buffer size scaling (A4 sawtooth) ──\n");
    print_header();

    for (int si = 0; si < n_sizes; si++) {
        int ns = sizes[si];
        float *output = (float *)malloc((size_t)ns * sizeof(float));
        gen_ctx ctx = { osc, output, ns, freq };

        bench_result r = run_bench(bench_generate, &ctx);
        print_result("generate_samples", ns, &r);
        check_result(&r, "buffer_size");

        free(output);
    }

    return 0;
}

static int bench_suite_waveforms(HexWave *osc) {
    int nsamples = 4096;
    float freq = 440.0f / g_sample_rate;
    float *output = (float *)malloc((size_t)nsamples * sizeof(float));

    printf("\n── Suite 2: Waveform shapes (4096 samples, A4) ──\n");
    print_header_simple();

    for (int i = 0; i < n_classic; i++) {
        waveform_t *w = &classic_waveforms[i];
        hexwave_create(osc, w->reflect, w->peak_time, w->half_height, w->zero_wait);
        /* warm up */
        hexwave_generate_samples(output, nsamples, osc, freq);
        hexwave_generate_samples(output, nsamples, osc, freq);

        gen_ctx ctx = { osc, output, nsamples, freq };
        bench_result r = run_bench(bench_generate, &ctx);
        print_result_simple(w->name, nsamples, &r);
        check_result(&r, w->name);
    }

    free(output);
    return 0;
}

static int bench_suite_frequency(HexWave *osc) {
    int nsamples = 4096;
    float freqs[] = {
        55.0f / g_sample_rate,      /* A1 — bass */
        220.0f / g_sample_rate,     /* A3 — mid */
        440.0f / g_sample_rate,     /* A4 — concert A */
        1760.0f / g_sample_rate,    /* A6 — high */
        8800.0f / g_sample_rate,    /* ~C9 — near Nyquist */
    };
    const char *freq_names[] = { "A1 (55Hz)", "A3 (220Hz)", "A4 (440Hz)",
                                  "A6 (1760Hz)", "C9 (8800Hz)" };
    int n_freqs = g_quick ? 3 : 5;

    printf("\n── Suite 3: Frequency sweep (4096 samples, sawtooth) ──\n");
    print_header_simple();

    hexwave_create(osc, 1, 0.0f, 0.0f, 0.0f); /* sawtooth */
    float *output = (float *)malloc((size_t)nsamples * sizeof(float));

    for (int fi = 0; fi < n_freqs; fi++) {
        /* Reset oscillator state for clean measurement */
        hexwave_create(osc, 1, 0.0f, 0.0f, 0.0f);
        hexwave_generate_samples(output, nsamples, osc, freqs[fi]);

        gen_ctx ctx = { osc, output, nsamples, freqs[fi] };
        bench_result r = run_bench(bench_generate, &ctx);
        print_result_simple(freq_names[fi], nsamples, &r);
        check_result(&r, freq_names[fi]);
    }

    free(output);
    return 0;
}

static int bench_suite_morphing(HexWave *osc) {
    int nsamples = 4096;
    float freq = 440.0f / g_sample_rate;
    float *output = (float *)malloc((size_t)nsamples * sizeof(float));
    int n_steps = g_quick ? 20 : 100;

    printf("\n── Suite 4: Morphing (peak_time sweep, %d steps, A4 sawtooth->square) ──\n", n_steps);
    printf("  %-28s %9s %9s %9s %10s %8s\n",
           "Operation", "Min(ms)", "Mean(ms)", "Max(ms)", "Msamp/s", "Iters");
    printf("  %-28s %9s %9s %9s %10s %8s\n",
           "----------------------------",
           "---------", "---------", "---------",
           "----------", "--------");

    /* Morph: sawtooth (peak_time=0) -> square (peak_time=0, half_height=1) */
    hexwave_create(osc, 1, 0.0f, 0.0f, 0.0f);
    hexwave_generate_samples(output, nsamples, osc, freq); /* warm up */

    double total_ms = 0;
    double best_ms = 1e18, worst_ms = 0;
    int iters_per_step = 0;

    for (int step = 0; step < n_steps; step++) {
        float t = (float)step / (float)(n_steps - 1);
        float peak_time = 0.0f; /* stays 0 for saw->square */
        float half_height = t;  /* 0 -> 1 */

        morph_ctx ctx = { osc, output, nsamples, freq, 1, peak_time, half_height, 0.0f };
        bench_result r = run_bench(bench_change_then_generate, &ctx);
        if (!r.ok) { printf("  >> FAIL at step %d\n", step); failures++; continue; }
        total_ms += r.mean_ms;
        if (r.min_ms < best_ms) best_ms = r.min_ms;
        if (r.max_ms > worst_ms) worst_ms = r.max_ms;
        iters_per_step = r.iters;
    }

    double mean_ms = total_ms / n_steps;
    double msamp = (double)nsamples / (mean_ms * 1e3);
    printf("  %-28s %9.3f %9.3f %9.3f %10.1f %8d\n",
           "change+generate (avg)", best_ms, mean_ms, worst_ms, msamp, iters_per_step);

    free(output);
    return 0;
}

/* ── CLI ─────────────────────────────────────────────────────────────── */

static void usage(void) {
    printf("Usage: bench_stb_hexwave [options]\n"
           "\n"
           "Options:\n"
           "  --quick         Skip larger sizes / fewer variants (fast smoke test)\n"
           "  --iters N       Fixed iteration count (bypass auto-calibration)\n"
           "  --warmup N      Warm-up iterations (default: 3)\n"
           "  --sr RATE       Sample rate in Hz (default: 44100)\n"
           "  --help          Show this help\n");
}

/* ── Main ────────────────────────────────────────────────────────────── */

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage();
            return 0;
        } else if (strcmp(argv[i], "--quick") == 0) {
            g_quick = 1;
        } else if (strcmp(argv[i], "--iters") == 0 && i + 1 < argc) {
            g_fixed_iters = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--warmup") == 0 && i + 1 < argc) {
            g_warmup = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--sr") == 0 && i + 1 < argc) {
            g_sample_rate = (float)atof(argv[++i]);
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            usage();
            return 1;
        }
    }

    printf("stb_hexwave.h benchmark\n");
    printf("Warm-up: %d  |  Timed: auto-calibrated (%s)\n",
           g_warmup, g_quick ? "quick" : "full");
    printf("Compiler flags: -O2 -DNDEBUG  |  Sample rate: %.0f Hz\n", g_sample_rate);

    /* Initialize the library */
    hexwave_init(32, 16, NULL);

    /* Create a reusable oscillator */
    HexWave osc;
    hexwave_create(&osc, 1, 0.0f, 0.0f, 0.0f);

    bench_suite_buffer_size(&osc);
    bench_suite_waveforms(&osc);
    bench_suite_frequency(&osc);
    bench_suite_morphing(&osc);

    hexwave_shutdown(NULL);

    printf("\n");
    if (failures)
        printf("%d benchmark(s) had failures.\n", failures);
    else
        printf("All benchmarks completed successfully.\n");

    return failures ? 1 : 0;
}
