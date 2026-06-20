/*
 * bench_stb_image_write.c — Performance benchmark for stb_image_write.h
 *
 * Measures encode throughput across formats, sizes, compression levels,
 * and channel counts. Uses memory-to-memory _to_func APIs.
 *
 * Build:
 *   clang -O2 -DNDEBUG -I.. -I. tests/bench_stb_image_write.c -lm -o build/bench_stb_image_write
 *
 * Usage:
 *   ./build/bench_stb_image_write [--quick] [--iters N] [--warmup N] [--help]
 */

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

/* ── Memory-buffer IO ────────────────────────────────────────────────── */

typedef struct {
    unsigned char *data;
    int len;
    int cap;
} mem_buf;

static void mem_write(void *ctx, void *data, int size) {
    mem_buf *b = (mem_buf *)ctx;
    if (b->len + size > b->cap) {
        b->cap = b->cap ? b->cap * 2 : 65536;
        while (b->len + size > b->cap)
            b->cap *= 2;
        b->data = (unsigned char *)realloc(b->data, (size_t)b->cap);
    }
    memcpy(b->data + b->len, data, (size_t)size);
    b->len += size;
}

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

static unsigned char *generate_rgba(int w, int h) {
    int n = w * h * 4;
    unsigned char *px = (unsigned char *)malloc((size_t)n);
    if (!px) return NULL;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int i = (y * w + x) * 4;
            px[i + 0] = (unsigned char)((x * 7 + y * 3) & 255);
            px[i + 1] = (unsigned char)((x * 3 + y * 5) & 255);
            px[i + 2] = (unsigned char)((x * 11 + y * 13) & 255);
            px[i + 3] = 200;
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
    int    out_bytes; /* encoded size (last iteration) */
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
    printf("  %-20s %5s %5s %9s %9s %9s %10s %8s %8s\n",
           "Format", "InW", "InH", "Min(ms)", "Mean(ms)", "Max(ms)",
           "MB/s", "EncSz", "Iters");
    printf("  %-20s %5s %5s %9s %9s %9s %10s %8s %8s\n",
           "--------------------", "-----", "-----",
           "---------", "---------", "---------",
           "----------", "--------", "--------");
}

static void print_result(const char *label, int w, int h, bench_result *r) {
    if (!r->ok) {
        printf("  %-20s %5d %5d    FAILED\n", label, w, h);
        return;
    }
    /* MB/s = input_size_bytes / (mean_seconds * 1e6) */
    double mbps = (double)(w * h * 3) / (r->mean_ms * 1e3);
    printf("  %-20s %5d %5d %9.3f %9.3f %9.3f %10.1f %8d %8d\n",
           label, w, h, r->min_ms, r->mean_ms, r->max_ms,
           mbps, r->out_bytes, r->iters);
}

static int failures = 0;

static void check_result(bench_result *r, const char *label) {
    if (!r->ok) { printf("  >> FAIL: %s\n", label); failures++; }
}

/* ── Benchmark context structs ───────────────────────────────────────── */

typedef struct {
    unsigned char *px;
    int w, h, comp;
    mem_buf *out;
} write_ctx;

typedef struct {
    float *px;
    int w, h, comp;
    mem_buf *out;
} write_float_ctx;

typedef struct {
    unsigned char *px;
    int w, h, comp;
    mem_buf *out;
    int quality;
} write_jpg_ctx;

typedef struct {
    unsigned char *px;
    int w, h, comp;
    mem_buf *out;
    int level;
} write_png_level_ctx;

typedef struct {
    unsigned char *px;
    int w, h, comp;
    mem_buf *out;
    int use_rle;
} write_tga_ctx;

/* ── Benchmark functions ─────────────────────────────────────────────── */

static int bench_write_png(void *vctx) {
    write_ctx *c = (write_ctx *)vctx;
    c->out->len = 0;
    return stbi_write_png_to_func(mem_write, c->out, c->w, c->h, c->comp,
                                   c->px, c->w * c->comp);
}

static int bench_write_bmp(void *vctx) {
    write_ctx *c = (write_ctx *)vctx;
    c->out->len = 0;
    return stbi_write_bmp_to_func(mem_write, c->out, c->w, c->h, c->comp, c->px);
}

static int bench_write_tga(void *vctx) {
    write_tga_ctx *c = (write_tga_ctx *)vctx;
    c->out->len = 0;
    int saved = stbi_write_tga_with_rle;
    stbi_write_tga_with_rle = c->use_rle;
    int ok = stbi_write_tga_to_func(mem_write, c->out, c->w, c->h, c->comp, c->px);
    stbi_write_tga_with_rle = saved;
    return ok;
}

static int bench_write_jpg(void *vctx) {
    write_jpg_ctx *c = (write_jpg_ctx *)vctx;
    c->out->len = 0;
    return stbi_write_jpg_to_func(mem_write, c->out, c->w, c->h, c->comp,
                                   c->px, c->quality);
}

static int bench_write_hdr(void *vctx) {
    write_float_ctx *c = (write_float_ctx *)vctx;
    c->out->len = 0;
    return stbi_write_hdr_to_func(mem_write, c->out, c->w, c->h, c->comp, c->px);
}

static int bench_write_png_level(void *vctx) {
    write_png_level_ctx *c = (write_png_level_ctx *)vctx;
    c->out->len = 0;
    int saved = stbi_write_png_compression_level;
    stbi_write_png_compression_level = c->level;
    int ok = stbi_write_png_to_func(mem_write, c->out, c->w, c->h, c->comp,
                                     c->px, c->w * c->comp);
    stbi_write_png_compression_level = saved;
    return ok;
}

/* ── Suites ──────────────────────────────────────────────────────────── */

static int bench_suite_format(void) {
    int sizes[][2] = { {64, 64}, {256, 256}, {512, 512}, {1024, 1024} };
    int n_sizes = g_quick ? 2 : 4;

    printf("\n── Suite 1: Format comparison (3ch, same image) ──\n");
    print_header();

    for (int si = 0; si < n_sizes; si++) {
        int w = sizes[si][0], h = sizes[si][1];
        unsigned char *rgb = generate_rgb(w, h);
        float *hdr = generate_rgb_float(w, h);
        mem_buf out = {0, 0, 0};
        char label[32];

        /* PNG */
        out.len = 0;
        {
            write_ctx ctx = { rgb, w, h, 3, &out };
            snprintf(label, sizeof(label), "PNG %dx%d", w, h);
            bench_result r = run_bench(bench_write_png, &ctx);
            r.out_bytes = out.len;
            print_result(label, w, h, &r);
            check_result(&r, label);
        }

        /* BMP */
        out.len = 0;
        {
            write_ctx ctx = { rgb, w, h, 3, &out };
            snprintf(label, sizeof(label), "BMP %dx%d", w, h);
            bench_result r = run_bench(bench_write_bmp, &ctx);
            r.out_bytes = out.len;
            print_result(label, w, h, &r);
            check_result(&r, label);
        }

        /* TGA */
        out.len = 0;
        {
            write_tga_ctx ctx = { rgb, w, h, 3, &out, 0 };
            snprintf(label, sizeof(label), "TGA %dx%d", w, h);
            bench_result r = run_bench(bench_write_tga, &ctx);
            r.out_bytes = out.len;
            print_result(label, w, h, &r);
            check_result(&r, label);
        }

        /* JPEG quality=80 */
        out.len = 0;
        {
            write_jpg_ctx ctx = { rgb, w, h, 3, &out, 80 };
            snprintf(label, sizeof(label), "JPEG80 %dx%d", w, h);
            bench_result r = run_bench(bench_write_jpg, &ctx);
            r.out_bytes = out.len;
            print_result(label, w, h, &r);
            check_result(&r, label);
        }

        /* HDR */
        out.len = 0;
        {
            write_float_ctx ctx = { hdr, w, h, 3, &out };
            snprintf(label, sizeof(label), "HDR %dx%d", w, h);
            bench_result r = run_bench(bench_write_hdr, &ctx);
            r.out_bytes = out.len;
            print_result(label, w, h, &r);
            check_result(&r, label);
        }

        free(rgb); free(hdr); free(out.data);
    }

    return 0;
}

static int bench_suite_png_levels(void) {
    int w = 512, h = 512;
    int levels[] = { 0, 1, 3, 5, 7, 8, 9 };
    int n_levels = g_quick ? 4 : 7;

    printf("\n── Suite 2: PNG compression levels (512x512, 3ch) ──\n");
    printf("  %-12s %9s %9s %9s %10s %8s\n",
           "Level", "Min(ms)", "Mean(ms)", "Max(ms)", "EncSz", "Iters");
    printf("  %-12s %9s %9s %9s %10s %8s\n",
           "------------", "---------", "---------", "---------",
           "----------", "--------");

    unsigned char *rgb = generate_rgb(w, h);
    mem_buf out = {0, 0, 0};

    for (int li = 0; li < n_levels; li++) {
        int level = levels[li];
        out.len = 0;
        char label[16];
        snprintf(label, sizeof(label), "level %d", level);

        write_png_level_ctx ctx = { rgb, w, h, 3, &out, level };
        bench_result r = run_bench(bench_write_png_level, &ctx);
        if (!r.ok) {
            printf("  %-12s    FAILED\n", label);
            failures++;
        } else {
            printf("  %-12s %9.3f %9.3f %9.3f %10d %8d\n",
                   label, r.min_ms, r.mean_ms, r.max_ms, out.len, r.iters);
        }
    }

    free(rgb); free(out.data);
    return 0;
}

static int bench_suite_jpg_quality(void) {
    int w = 512, h = 512;
    int qualities[] = { 1, 10, 25, 50, 75, 90, 100 };
    int n_q = g_quick ? 4 : 7;

    printf("\n── Suite 3: JPEG quality sweep (512x512, 3ch) ──\n");
    printf("  %-10s %9s %9s %9s %10s %8s\n",
           "Quality", "Min(ms)", "Mean(ms)", "Max(ms)", "EncSz", "Iters");
    printf("  %-10s %9s %9s %9s %10s %8s\n",
           "----------", "---------", "---------", "---------",
           "----------", "--------");

    unsigned char *rgb = generate_rgb(w, h);
    mem_buf out = {0, 0, 0};

    for (int qi = 0; qi < n_q; qi++) {
        int q = qualities[qi];
        out.len = 0;
        char label[16];
        snprintf(label, sizeof(label), "q=%d", q);

        write_jpg_ctx ctx = { rgb, w, h, 3, &out, q };
        bench_result r = run_bench(bench_write_jpg, &ctx);
        if (!r.ok) {
            printf("  %-10s    FAILED\n", label);
            failures++;
        } else {
            printf("  %-10s %9.3f %9.3f %9.3f %10d %8d\n",
                   label, r.min_ms, r.mean_ms, r.max_ms, out.len, r.iters);
        }
    }

    free(rgb); free(out.data);
    return 0;
}

static int bench_suite_size_scaling(void) {
    int sizes[][2] = { {64, 64}, {128, 128}, {256, 256}, {512, 512}, {1024, 1024} };
    int n_sizes = g_quick ? 3 : 5;

    printf("\n── Suite 4: Size scaling (PNG, 3ch) ──\n");
    print_header();

    for (int si = 0; si < n_sizes; si++) {
        int w = sizes[si][0], h = sizes[si][1];
        unsigned char *rgb = generate_rgb(w, h);
        mem_buf out = {0, 0, 0};

        write_ctx ctx = { rgb, w, h, 3, &out };
        bench_result r = run_bench(bench_write_png, &ctx);
        r.out_bytes = out.len;
        char label[32];
        snprintf(label, sizeof(label), "PNG %dx%d", w, h);
        print_result(label, w, h, &r);
        check_result(&r, label);

        free(rgb); free(out.data);
    }

    return 0;
}

static int bench_suite_channels(void) {
    int w = 512, h = 512;
    int comps[] = { 1, 3, 4 };
    const char *comp_names[] = { "1ch grey", "3ch RGB", "4ch RGBA" };

    printf("\n── Suite 5: Channel count (PNG 512x512) ──\n");
    print_header();

    for (int ci = 0; ci < 3; ci++) {
        int comp = comps[ci];
        unsigned char *px = (comp == 4) ? generate_rgba(w, h) : generate_rgb(w, h);
        if (comp == 1) {
            /* generate greyscale: take R channel */
            unsigned char *grey = malloc(w * h);
            for (int i = 0; i < w * h; i++)
                grey[i] = px[i * 3];
            free(px);
            px = grey;
        }
        mem_buf out = {0, 0, 0};

        write_ctx ctx = { px, w, h, comp, &out };
        bench_result r = run_bench(bench_write_png, &ctx);
        r.out_bytes = out.len;
        print_result(comp_names[ci], w, h, &r);
        check_result(&r, comp_names[ci]);

        free(px); free(out.data);
    }

    return 0;
}

static int bench_suite_tga_rle(void) {
    int w = 512, h = 512;

    printf("\n── Suite 6: TGA RLE (512x512, 3ch) ──\n");
    printf("  %-12s %9s %9s %9s %10s %8s\n",
           "Mode", "Min(ms)", "Mean(ms)", "Max(ms)", "EncSz", "Iters");
    printf("  %-12s %9s %9s %9s %10s %8s\n",
           "------------", "---------", "---------", "---------",
           "----------", "--------");

    unsigned char *rgb = generate_rgb(w, h);
    mem_buf out = {0, 0, 0};

    /* TGA without RLE */
    {
        write_tga_ctx ctx = { rgb, w, h, 3, &out, 0 };
        bench_result r = run_bench(bench_write_tga, &ctx);
        if (!r.ok) { printf("  TGA no RLE    FAILED\n"); failures++; }
        else printf("  TGA no RLE  %9.3f %9.3f %9.3f %10d %8d\n",
                    r.min_ms, r.mean_ms, r.max_ms, out.len, r.iters);
    }

    /* TGA with RLE */
    out.len = 0;
    {
        write_tga_ctx ctx = { rgb, w, h, 3, &out, 1 };
        bench_result r = run_bench(bench_write_tga, &ctx);
        if (!r.ok) { printf("  TGA RLE      FAILED\n"); failures++; }
        else printf("  TGA RLE    %9.3f %9.3f %9.3f %10d %8d\n",
                    r.min_ms, r.mean_ms, r.max_ms, out.len, r.iters);
    }

    free(rgb); free(out.data);
    return 0;
}

/* ── CLI ─────────────────────────────────────────────────────────────── */

static void usage(void) {
    printf("Usage: bench_stb_image_write [options]\n"
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

    printf("stb_image_write.h benchmark\n");
    printf("Warm-up: %d  |  Timed: auto-calibrated (%s)\n",
           g_warmup, g_quick ? "quick" : "full");
    printf("Compiler flags: -O2 -DNDEBUG\n");

    bench_suite_format();
    bench_suite_png_levels();
    bench_suite_jpg_quality();
    bench_suite_size_scaling();
    bench_suite_channels();
    bench_suite_tga_rle();

    printf("\n");
    if (failures)
        printf("%d benchmark(s) had failures.\n", failures);
    else
        printf("All benchmarks completed successfully.\n");

    return failures ? 1 : 0;
}
