/*
 * bench_stb_image_resize2.c — Performance benchmark for stb_image_resize2.h
 *
 * Measures resize throughput across API styles, filters, edge modes,
 * scale directions, and channel counts.
 *
 * Build:
 *   clang -O2 -DNDEBUG -I.. -I. tests/bench_stb_image_resize2.c -lm -o build/bench_stb_image_resize2
 *
 * Usage:
 *   ./build/bench_stb_image_resize2 [--quick] [--iters N] [--warmup N] [--help]
 */

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

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

/* ── Synthetic image generation ──────────────────────────────────────── */

static unsigned char *generate_rgb(int w, int h) {
    int n = w * h * 3;
    unsigned char *px = (unsigned char *)malloc((size_t)n);
    if (!px) return NULL;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int i = (y * w + x) * 3;
            px[i + 0] = (unsigned char)((x * 7 + y * 3) & 255);
            px[i + 1] = (unsigned char)((x * 3 + y * 5) & 255);
            px[i + 2] = (unsigned char)((x * 11 + y * 13) & 255);
        }
    return px;
}

static float *generate_rgb_float(int w, int h) {
    int n = w * h * 3;
    float *px = (float *)malloc((size_t)n * sizeof(float));
    if (!px) return NULL;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int i = (y * w + x) * 3;
            px[i + 0] = ((x * 7 + y * 3) & 255) / 255.0f;
            px[i + 1] = ((x * 3 + y * 5) & 255) / 255.0f;
            px[i + 2] = ((x * 11 + y * 13) & 255) / 255.0f;
        }
    return px;
}

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
    printf("  %-24s %5s %5s %9s %9s %9s %10s %8s\n",
           "Operation", "InW", "OutW", "Min(ms)", "Mean(ms)", "Max(ms)", "Mp/s", "Iters");
    printf("  %-24s %5s %5s %9s %9s %9s %10s %8s\n",
           "------------------------", "-----", "-----",
           "---------", "---------", "---------", "----------", "--------");
}

static void print_result(const char *label, int in_w, int out_w, bench_result *r) {
    if (!r->ok) {
        printf("  %-24s %5d %5d    FAILED\n", label, in_w, out_w);
        return;
    }
    /* Mp/s = output pixels / (mean_seconds * 1e6) */
    double mp = (double)out_w * (double)out_w / (r->mean_ms * 1e3);
    printf("  %-24s %5d %5d %9.3f %9.3f %9.3f %10.1f %8d\n",
           label, in_w, out_w, r->min_ms, r->mean_ms, r->max_ms, mp, r->iters);
}

static int failures = 0;

static void check_result(bench_result *r, const char *label) {
    if (!r->ok) { printf("  >> FAIL: %s\n", label); failures++; }
}

/* ── Benchmark context structs ───────────────────────────────────────── */

typedef struct {
    unsigned char *in;
    unsigned char *out;
    int in_w, in_h, out_w, out_h;
    stbir_pixel_layout layout;
} resize_uint8_ctx;

typedef struct {
    float *in;
    float *out;
    int in_w, in_h, out_w, out_h;
    stbir_pixel_layout layout;
} resize_float_ctx;

typedef struct {
    unsigned char *in;
    unsigned char *out;
    int in_w, in_h, out_w, out_h;
    stbir_pixel_layout layout;
    stbir_filter filter;
    stbir_edge edge;
} resize_ext_ctx;

/* ── Benchmark functions ─────────────────────────────────────────────── */

static int bench_uint8_srgb(void *vctx) {
    resize_uint8_ctx *c = (resize_uint8_ctx *)vctx;
    unsigned char *r = stbir_resize_uint8_srgb(
        c->in, c->in_w, c->in_h, c->in_w * 3,
        c->out, c->out_w, c->out_h, c->out_w * 3,
        c->layout);
    return r != NULL || c->out != NULL;
}

static int bench_uint8_linear(void *vctx) {
    resize_uint8_ctx *c = (resize_uint8_ctx *)vctx;
    unsigned char *r = stbir_resize_uint8_linear(
        c->in, c->in_w, c->in_h, c->in_w * 3,
        c->out, c->out_w, c->out_h, c->out_w * 3,
        c->layout);
    return r != NULL || c->out != NULL;
}

static int bench_float_linear(void *vctx) {
    resize_float_ctx *c = (resize_float_ctx *)vctx;
    float *r = stbir_resize_float_linear(
        c->in, c->in_w, c->in_h, c->in_w * 3 * (int)sizeof(float),
        c->out, c->out_w, c->out_h, c->out_w * 3 * (int)sizeof(float),
        c->layout);
    return r != NULL || c->out != NULL;
}

static int bench_extended(void *vctx) {
    resize_ext_ctx *c = (resize_ext_ctx *)vctx;
    STBIR_RESIZE resize;
    stbir_resize_init(&resize,
        c->in, c->in_w, c->in_h, c->in_w * 3,
        c->out, c->out_w, c->out_h, c->out_w * 3,
        c->layout, STBIR_TYPE_UINT8);
    stbir_set_filters(&resize, c->filter, c->filter);
    stbir_set_edgemodes(&resize, c->edge, c->edge);
    if (!stbir_build_samplers(&resize))
        return 0;
    int ok = stbir_resize_extended(&resize);
    stbir_free_samplers(&resize);
    return ok;
}

/* ── Suites ──────────────────────────────────────────────────────────── */

static int bench_suite_simple(void) {
    int sizes[][2] = { {64, 128}, {128, 256}, {256, 512}, {512, 256}, {1024, 512}, {1024, 1024} };
    int n_sizes = g_quick ? 3 : 6;
    stbir_pixel_layout layout = STBIR_RGB;

    printf("\n── Suite 1: Simple API (stbir_resize_uint8_srgb / linear / float) ──\n");
    print_header();

    for (int si = 0; si < n_sizes; si++) {
        int iw = sizes[si][0], ow = sizes[si][1];
        char label[64];

        /* uint8 srgb */
        {
            unsigned char *in = generate_rgb(iw, iw);
            unsigned char *out = (unsigned char *)malloc((size_t)(ow * ow * 3));
            resize_uint8_ctx ctx = { in, out, iw, iw, ow, ow, layout };
            snprintf(label, sizeof(label), "uint8_srgb %dx%d->%d", iw, iw, ow);
            bench_result r = run_bench(bench_uint8_srgb, &ctx);
            print_result(label, iw, ow, &r);
            check_result(&r, label);
            free(in); free(out);
        }

        /* uint8 linear */
        {
            unsigned char *in = generate_rgb(iw, iw);
            unsigned char *out = (unsigned char *)malloc((size_t)(ow * ow * 3));
            resize_uint8_ctx ctx = { in, out, iw, iw, ow, ow, layout };
            snprintf(label, sizeof(label), "uint8_linear %dx%d->%d", iw, iw, ow);
            bench_result r = run_bench(bench_uint8_linear, &ctx);
            print_result(label, iw, ow, &r);
            check_result(&r, label);
            free(in); free(out);
        }

        /* float linear */
        {
            float *in = generate_rgb_float(iw, iw);
            float *out = (float *)malloc((size_t)(ow * ow * 3) * sizeof(float));
            resize_float_ctx ctx = { in, out, iw, iw, ow, ow, layout };
            snprintf(label, sizeof(label), "float_linear %dx%d->%d", iw, iw, ow);
            bench_result r = run_bench(bench_float_linear, &ctx);
            print_result(label, iw, ow, &r);
            check_result(&r, label);
            free(in); free(out);
        }
    }

    return 0;
}

static int bench_suite_filter(void) {
    int in_w = 512, out_w = 256; /* downscale */
    stbir_filter filters[] = {
        STBIR_FILTER_BOX, STBIR_FILTER_TRIANGLE,
        STBIR_FILTER_CUBICBSPLINE, STBIR_FILTER_CATMULLROM, STBIR_FILTER_MITCHELL
    };
    const char *filter_names[] = {
        "BOX", "TRIANGLE", "CUBICBSPLINE", "CATMULLROM", "MITCHELL"
    };
    int n_filters = g_quick ? 3 : 5;

    printf("\n── Suite 2: Filter comparison (512->256, uint8 linear) ──\n");
    print_header();

    unsigned char *in = generate_rgb(in_w, in_w);
    unsigned char *out = (unsigned char *)malloc((size_t)(out_w * out_w * 3));

    for (int fi = 0; fi < n_filters; fi++) {
        resize_ext_ctx ctx = {
            in, out, in_w, in_w, out_w, out_w,
            STBIR_RGB, filters[fi], STBIR_EDGE_CLAMP
        };
        bench_result r = run_bench(bench_extended, &ctx);
        print_result(filter_names[fi], in_w, out_w, &r);
        check_result(&r, filter_names[fi]);
    }

    free(in); free(out);
    return 0;
}

static int bench_suite_edge(void) {
    int in_w = 256, out_w = 512; /* upscale to accentuate edge handling */
    stbir_edge edges[] = { STBIR_EDGE_CLAMP, STBIR_EDGE_REFLECT, STBIR_EDGE_WRAP, STBIR_EDGE_ZERO };
    const char *edge_names[] = { "CLAMP", "REFLECT", "WRAP", "ZERO" };
    int n_edges = g_quick ? 2 : 4;

    printf("\n── Suite 3: Edge mode comparison (256->512, uint8 linear, TRIANGLE) ──\n");
    print_header();

    unsigned char *in = generate_rgb(in_w, in_w);
    unsigned char *out = (unsigned char *)malloc((size_t)(out_w * out_w * 3));

    for (int ei = 0; ei < n_edges; ei++) {
        resize_ext_ctx ctx = {
            in, out, in_w, in_w, out_w, out_w,
            STBIR_RGB, STBIR_FILTER_TRIANGLE, edges[ei]
        };
        bench_result r = run_bench(bench_extended, &ctx);
        print_result(edge_names[ei], in_w, out_w, &r);
        check_result(&r, edge_names[ei]);
    }

    free(in); free(out);
    return 0;
}

static int bench_suite_scale_direction(void) {
    int pairs[][2] = { {128, 512}, {256, 512}, {512, 512}, {512, 256}, {512, 128} };
    int n_pairs = g_quick ? 3 : 5;
    const char *labels[] = { "4x upscale", "2x upscale", "1x (same)", "2x downscale", "4x downscale" };

    printf("\n── Suite 4: Scale direction (uint8 srgb, 3ch) ──\n");
    print_header();

    for (int pi = 0; pi < n_pairs; pi++) {
        int iw = pairs[pi][0], ow = pairs[pi][1];
        unsigned char *in = generate_rgb(iw, iw);
        unsigned char *out = (unsigned char *)malloc((size_t)(ow * ow * 3));
        resize_uint8_ctx ctx = { in, out, iw, iw, ow, ow, STBIR_RGB };

        bench_result r = run_bench(bench_uint8_srgb, &ctx);
        print_result(labels[pi], iw, ow, &r);
        check_result(&r, labels[pi]);
        free(in); free(out);
    }

    return 0;
}

/* ── CLI ─────────────────────────────────────────────────────────────── */

static void usage(void) {
    printf("Usage: bench_stb_image_resize2 [options]\n"
           "\n"
           "Options:\n"
           "  --quick         Skip larger sizes / fewer variants (fast smoke test)\n"
           "  --iters N       Fixed iteration count (bypass auto-calibration)\n"
           "  --warmup N      Warm-up iterations (default: 3)\n"
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
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            usage();
            return 1;
        }
    }

    printf("stb_image_resize2.h benchmark\n");
    printf("Warm-up: %d  |  Timed: auto-calibrated (%s)\n",
           g_warmup, g_quick ? "quick" : "full");
    printf("Compiler flags: -O2 -DNDEBUG\n");

    bench_suite_simple();
    bench_suite_filter();
    bench_suite_edge();
    bench_suite_scale_direction();

    printf("\n");
    if (failures)
        printf("%d benchmark(s) had failures.\n", failures);
    else
        printf("All benchmarks completed successfully.\n");

    return failures ? 1 : 0;
}
