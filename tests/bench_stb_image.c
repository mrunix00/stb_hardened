/*
 * bench_stb_image.c — Performance benchmark for stb_image.h
 *
 * Measures decode throughput across formats, sizes, and channel counts.
 * Uses stb_image_write.h to generate synthetic test images at runtime,
 * then benchmarks stbi_load_from_memory on the encoded data.
 *
 * Build:
 *   clang -O2 -DNDEBUG -I.. -I. tests/bench_stb_image.c -lm -o build/bench_stb_image
 *
 * Usage:
 *   ./build/bench_stb_image [--quick] [--iters N] [--warmup N] [--help]
 */

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

/* ── Timing ──────────────────────────────────────────────────────────── */

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/* ── Options ─────────────────────────────────────────────────────────── */

static int g_quick     = 0;   /* skip larger sizes */
static int g_fixed_iters = 0; /* 0 = auto-calibrate */
static int g_warmup    = 3;

/* ── Synthetic image generation ──────────────────────────────────────── */

/*
 * Generate a 256x256 gradient + noise image (3-component RGB) for
 * encode-then-decode testing.  The deterministic pattern ensures
 * consistent encoded sizes and avoids degenerate (all-zero / all-0xFF)
 * images that compress to tiny blobs.
 */
static unsigned char *generate_pixels(int w, int h, int comp) {
    int n = w * h * comp;
    unsigned char *px = (unsigned char *)malloc((size_t)n);
    if (!px) return NULL;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int idx = (y * w + x) * comp;
            px[idx + 0] = (unsigned char)((x * 7 + y * 3) & 255);
            px[idx + 1] = (unsigned char)((x * 3 + y * 5) & 255);
            if (comp >= 3)
                px[idx + 2] = (unsigned char)((x * 11 + y * 13) & 255);
            if (comp >= 4)
                px[idx + 3] = 200;
        }
    return px;
}

static unsigned char *generate_pixels_float(int w, int h, int comp, float **out) {
    int n = w * h * comp;
    float *px = (float *)malloc((size_t)n * sizeof(float));
    if (!px) return NULL;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int idx = (y * w + x) * comp;
            px[idx + 0] = ((x * 7 + y * 3) & 255) / 255.0f;
            px[idx + 1] = ((x * 3 + y * 5) & 255) / 255.0f;
            if (comp >= 3)
                px[idx + 2] = ((x * 11 + y * 13) & 255) / 255.0f;
            if (comp >= 4)
                px[idx + 3] = 200 / 255.0f;
        }
    *out = px;
    return NULL; /* caller uses the float* directly */
}

/* ── Encode helpers ──────────────────────────────────────────────────── */

static mem_buf encode_png(unsigned char *px, int w, int h, int comp) {
    mem_buf b = {0, 0, 0};
    stbi_write_png_to_func(mem_write, &b, w, h, comp, px, w * comp);
    return b;
}

static mem_buf encode_jpeg(unsigned char *px, int w, int h, int comp, int quality) {
    mem_buf b = {0, 0, 0};
    stbi_write_jpg_to_func(mem_write, &b, w, h, comp, px, quality);
    return b;
}

static mem_buf encode_bmp(unsigned char *px, int w, int h, int comp) {
    mem_buf b = {0, 0, 0};
    stbi_write_bmp_to_func(mem_write, &b, w, h, comp, px);
    return b;
}

static mem_buf encode_tga(unsigned char *px, int w, int h, int comp) {
    mem_buf b = {0, 0, 0};
    stbi_write_tga_to_func(mem_write, &b, w, h, comp, px);
    return b;
}

static mem_buf encode_hdr(float *px, int w, int h, int comp) {
    mem_buf b = {0, 0, 0};
    stbi_write_hdr_to_func(mem_write, &b, w, h, comp, px);
    return b;
}

/* ── Benchmark core ──────────────────────────────────────────────────── */

typedef struct {
    double min_ms;
    double mean_ms;
    double max_ms;
    int    iters;
    int    ok;
} bench_result;

/*
 * Auto-calibrate iteration count: run 5 probes, compute per-call time,
 * then set iterations such that the timed window is ~500ms.
 */
static bench_result run_bench_decode(
    const unsigned char *buf, int buf_len,
    int req_comp, int want_w, int want_h)
{
    bench_result r = {0};
    int calib = 5;
    uint64_t t0, t1;
    uint64_t per_run;
    int iterations;

    /* calibration */
    t0 = now_ns();
    for (int i = 0; i < calib; i++) {
        int w, h, c;
        unsigned char *p = stbi_load_from_memory(buf, buf_len, &w, &h, &c, req_comp);
        if (!p) { r.ok = 0; return r; }
        stbi_image_free(p);
    }
    t1 = now_ns();
    per_run = (t1 - t0) / (uint64_t)calib;

    if (g_fixed_iters > 0) {
        iterations = g_fixed_iters;
    } else {
        iterations = (per_run > 0) ? (int)(500000000ULL / per_run) : 500;
        if (iterations < 5) iterations = 5;
        if (iterations > 500) iterations = 500;
    }

    /* warm-up */
    for (int i = 0; i < g_warmup; i++) {
        int w, h, c;
        unsigned char *p = stbi_load_from_memory(buf, buf_len, &w, &h, &c, req_comp);
        if (!p) { r.ok = 0; return r; }
        stbi_image_free(p);
    }

    /* timed run */
    double total = 0;
    double best = 1e18, worst = 0;
    t0 = now_ns();
    for (int i = 0; i < iterations; i++) {
        int w, h, c;
        unsigned char *p = stbi_load_from_memory(buf, buf_len, &w, &h, &c, req_comp);
        if (!p) { r.ok = 0; return r; }
        stbi_image_free(p);
    }
    t1 = now_ns();
    total = (double)(t1 - t0);
    double mean_ns = total / iterations;
    /* We don't have per-iteration min/max from above; approximate from total */
    r.min_ms  = total / iterations / 1e6; /* will be overwritten below */
    r.mean_ms = mean_ns / 1e6;
    r.max_ms  = r.mean_ms; /* placeholder; per-iteration re-run for min/max */
    r.iters   = iterations;
    r.ok      = 1;

    /* Re-run with per-iteration timing for min/max (short pass) */
    int detail_iters = iterations < 20 ? iterations : 20;
    best = 1e18; worst = 0;
    for (int i = 0; i < detail_iters; i++) {
        uint64_t a = now_ns();
        int w, h, c;
        unsigned char *p = stbi_load_from_memory(buf, buf_len, &w, &h, &c, req_comp);
        stbi_image_free(p);
        uint64_t b = now_ns();
        double ms = (double)(b - a) / 1e6;
        if (ms < best) best = ms;
        if (ms > worst) worst = ms;
    }
    r.min_ms = best;
    r.max_ms = worst;

    return r;
}

static bench_result run_bench_info(
    const unsigned char *buf, int buf_len)
{
    bench_result r = {0};
    int calib = 5;
    uint64_t t0, t1, per_run;
    int iterations;

    t0 = now_ns();
    for (int i = 0; i < calib; i++) {
        int w, h, c;
        if (!stbi_info_from_memory(buf, buf_len, &w, &h, &c)) { r.ok = 0; return r; }
    }
    t1 = now_ns();
    per_run = (t1 - t0) / (uint64_t)calib;

    if (g_fixed_iters > 0)
        iterations = g_fixed_iters;
    else {
        iterations = (per_run > 0) ? (int)(500000000ULL / per_run) : 5000;
        if (iterations < 10) iterations = 10;
        if (iterations > 5000) iterations = 5000;
    }

    /* warm-up */
    for (int i = 0; i < g_warmup; i++) {
        int w, h, c;
        stbi_info_from_memory(buf, buf_len, &w, &h, &c);
    }

    double best = 1e18, worst = 0;
    t0 = now_ns();
    for (int i = 0; i < iterations; i++) {
        int w, h, c;
        stbi_info_from_memory(buf, buf_len, &w, &h, &c);
    }
    t1 = now_ns();
    r.mean_ms = (double)(t1 - t0) / iterations / 1e6;
    r.iters   = iterations;

    int detail_iters = iterations < 50 ? iterations : 50;
    best = 1e18; worst = 0;
    for (int i = 0; i < detail_iters; i++) {
        uint64_t a = now_ns();
        int w, h, c;
        stbi_info_from_memory(buf, buf_len, &w, &h, &c);
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

static bench_result run_bench_load16(
    const unsigned char *buf, int buf_len,
    int req_comp)
{
    bench_result r = {0};
    int calib = 5;
    uint64_t t0, t1, per_run;
    int iterations;

    t0 = now_ns();
    for (int i = 0; i < calib; i++) {
        int w, h, c;
        stbi_us *p = stbi_load_16_from_memory(buf, buf_len, &w, &h, &c, req_comp);
        if (!p) { r.ok = 0; return r; }
        stbi_image_free(p);
    }
    t1 = now_ns();
    per_run = (t1 - t0) / (uint64_t)calib;

    if (g_fixed_iters > 0)
        iterations = g_fixed_iters;
    else {
        iterations = (per_run > 0) ? (int)(500000000ULL / per_run) : 500;
        if (iterations < 5) iterations = 5;
        if (iterations > 500) iterations = 500;
    }

    for (int i = 0; i < g_warmup; i++) {
        int w, h, c;
        stbi_us *p = stbi_load_16_from_memory(buf, buf_len, &w, &h, &c, req_comp);
        if (!p) { r.ok = 0; return r; }
        stbi_image_free(p);
    }

    double best = 1e18, worst = 0;
    t0 = now_ns();
    for (int i = 0; i < iterations; i++) {
        int w, h, c;
        stbi_us *p = stbi_load_16_from_memory(buf, buf_len, &w, &h, &c, req_comp);
        stbi_image_free(p);
    }
    t1 = now_ns();
    r.mean_ms = (double)(t1 - t0) / iterations / 1e6;
    r.iters   = iterations;

    int detail_iters = iterations < 20 ? iterations : 20;
    best = 1e18; worst = 0;
    for (int i = 0; i < detail_iters; i++) {
        uint64_t a = now_ns();
        int w, h, c;
        stbi_us *p = stbi_load_16_from_memory(buf, buf_len, &w, &h, &c, req_comp);
        stbi_image_free(p);
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

static bench_result run_bench_loadf(
    const unsigned char *buf, int buf_len,
    int req_comp)
{
    bench_result r = {0};
    int calib = 5;
    uint64_t t0, t1, per_run;
    int iterations;

    t0 = now_ns();
    for (int i = 0; i < calib; i++) {
        int w, h, c;
        float *p = stbi_loadf_from_memory(buf, buf_len, &w, &h, &c, req_comp);
        if (!p) { r.ok = 0; return r; }
        stbi_image_free(p);
    }
    t1 = now_ns();
    per_run = (t1 - t0) / (uint64_t)calib;

    if (g_fixed_iters > 0)
        iterations = g_fixed_iters;
    else {
        iterations = (per_run > 0) ? (int)(500000000ULL / per_run) : 500;
        if (iterations < 5) iterations = 5;
        if (iterations > 500) iterations = 500;
    }

    for (int i = 0; i < g_warmup; i++) {
        int w, h, c;
        float *p = stbi_loadf_from_memory(buf, buf_len, &w, &h, &c, req_comp);
        if (!p) { r.ok = 0; return r; }
        stbi_image_free(p);
    }

    double best = 1e18, worst = 0;
    t0 = now_ns();
    for (int i = 0; i < iterations; i++) {
        int w, h, c;
        float *p = stbi_loadf_from_memory(buf, buf_len, &w, &h, &c, req_comp);
        stbi_image_free(p);
    }
    t1 = now_ns();
    r.mean_ms = (double)(t1 - t0) / iterations / 1e6;
    r.iters   = iterations;

    int detail_iters = iterations < 20 ? iterations : 20;
    best = 1e18; worst = 0;
    for (int i = 0; i < detail_iters; i++) {
        uint64_t a = now_ns();
        int w, h, c;
        float *p = stbi_loadf_from_memory(buf, buf_len, &w, &h, &c, req_comp);
        stbi_image_free(p);
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
    printf("  %-8s %-10s %-7s %9s %9s %9s %10s %8s\n",
           "Format", "Size", "ReqCmp", "Min(ms)", "Mean(ms)", "Max(ms)", "MP/s", "EncSz");
    printf("  %-8s %-10s %-7s %9s %9s %9s %10s %8s\n",
           "--------", "----------", "-------", "---------", "---------", "---------", "----------", "--------");
}

static void print_result(const char *fmt, const char *size_str, int req_comp,
                         bench_result *r, int w, int h, int enc_sz) {
    if (!r->ok) {
        printf("  %-8s %-10s %-7s    FAILED\n", fmt, size_str,
               req_comp < 0 ? "auto" : (req_comp == 0 ? "0" : (req_comp == 1 ? "1" :
               (req_comp == 3 ? "3" : "4"))));
        return;
    }
    double mp = (double)w * (double)h / (r->mean_ms * 1e3);
    printf("  %-8s %-10s %-7s %9.3f %9.3f %9.3f %10.1f %8d\n",
           fmt, size_str,
           req_comp < 0 ? "auto" : (req_comp == 0 ? "0" : (req_comp == 1 ? "1" :
           (req_comp == 3 ? "3" : "4"))),
           r->min_ms, r->mean_ms, r->max_ms, mp, enc_sz);
}

static void print_result_info(const char *fmt, const char *size_str,
                              bench_result *r) {
    if (!r->ok) {
        printf("  %-8s %-10s %-7s    FAILED\n", fmt, size_str, "n/a");
        return;
    }
    printf("  %-8s %-10s %-7s %9.3f %9.3f %9.3f %10s %8s\n",
           fmt, size_str, "n/a",
           r->min_ms, r->mean_ms, r->max_ms, "-", "-");
}

/* ── Benchmark suites ────────────────────────────────────────────────── */

static int failures = 0;

static void check_result(bench_result *r, const char *label) {
    if (!r->ok) {
        printf("  >> FAIL: %s\n", label);
        failures++;
    }
}

/*
 * Suite 1: Format comparison — same size, all formats, 3-channel RGB
 */
static int bench_suite_format(void) {
    int sizes[] = { 64, 256, 512 };
    int n_sizes = g_quick ? 1 : 3;
    int comp = 3;

    printf("\n── Suite 1: Format comparison (same size, %d channels) ──\n", comp);
    print_header();

    for (int si = 0; si < n_sizes; si++) {
        int sz = sizes[si];
        char label[32];
        snprintf(label, sizeof(label), "%dx%d", sz, sz);

        unsigned char *px = generate_pixels(sz, sz, comp);

        /* PNG */
        {
            mem_buf b = encode_png(px, sz, sz, comp);
            bench_result r = run_bench_decode(b.data, b.len, comp, sz, sz);
            print_result("PNG", label, comp, &r, sz, sz, b.len);
            check_result(&r, "PNG decode");
            free(b.data);
        }

        /* JPEG quality=80 */
        {
            mem_buf b = encode_jpeg(px, sz, sz, comp, 80);
            bench_result r = run_bench_decode(b.data, b.len, comp, sz, sz);
            print_result("JPEG", label, comp, &r, sz, sz, b.len);
            check_result(&r, "JPEG decode");
            free(b.data);
        }

        /* BMP */
        {
            mem_buf b = encode_bmp(px, sz, sz, comp);
            bench_result r = run_bench_decode(b.data, b.len, comp, sz, sz);
            print_result("BMP", label, comp, &r, sz, sz, b.len);
            check_result(&r, "BMP decode");
            free(b.data);
        }

        /* TGA */
        {
            mem_buf b = encode_tga(px, sz, sz, comp);
            bench_result r = run_bench_decode(b.data, b.len, comp, sz, sz);
            print_result("TGA", label, comp, &r, sz, sz, b.len);
            check_result(&r, "TGA decode");
            free(b.data);
        }

        free(px);
    }

    return 0;
}

/*
 * Suite 2: Size scaling — PNG at increasing resolutions
 */
static int bench_suite_size_scaling(void) {
    int sizes[] = { 64, 128, 256, 512, 1024 };
    int n_sizes = g_quick ? 3 : 5;
    int comp = 3;

    printf("\n── Suite 2: Size scaling (PNG, %d channels) ──\n", comp);
    print_header();

    for (int si = 0; si < n_sizes; si++) {
        int sz = sizes[si];
        char label[32];
        snprintf(label, sizeof(label), "%dx%d", sz, sz);

        unsigned char *px = generate_pixels(sz, sz, comp);
        mem_buf b = encode_png(px, sz, sz, comp);
        bench_result r = run_bench_decode(b.data, b.len, comp, sz, sz);
        print_result("PNG", label, comp, &r, sz, sz, b.len);
        check_result(&r, label);
        free(b.data);
        free(px);
    }

    return 0;
}

/*
 * Suite 3: Channel conversion — same image, req_comp=0,1,3,4
 */
static int bench_suite_channels(void) {
    int sz = 512;
    int comp = 3;
    int req_comps[] = { 0, 1, 3, 4 };

    printf("\n── Suite 3: Channel conversion (PNG %dx%d, vary req_comp) ──\n", sz, sz);
    print_header();

    unsigned char *px = generate_pixels(sz, sz, comp);
    mem_buf b = encode_png(px, sz, sz, comp);

    for (int i = 0; i < 4; i++) {
        bench_result r = run_bench_decode(b.data, b.len, req_comps[i], sz, sz);
        print_result("PNG", "512x512", req_comps[i], &r, sz, sz, b.len);
        if (!r.ok) {
            printf("  >> FAIL: req_comp=%d\n", req_comps[i]);
            failures++;
        }
    }

    free(b.data);
    free(px);
    return 0;
}

/*
 * Suite 4: Header-only decode (stbi_info)
 */
static int bench_suite_info(void) {
    int sizes[] = { 64, 256, 512, 1024 };
    int n_sizes = g_quick ? 2 : 4;
    int comp = 3;

    printf("\n── Suite 4: Header-only decode (stbi_info) ──\n");
    printf("  %-8s %-10s %9s %9s %9s\n",
           "Format", "Size", "Min(ms)", "Mean(ms)", "Max(ms)");
    printf("  %-8s %-10s %9s %9s %9s\n",
           "--------", "----------", "---------", "---------", "---------");

    for (int si = 0; si < n_sizes; si++) {
        int sz = sizes[si];
        char label[32];
        snprintf(label, sizeof(label), "%dx%d", sz, sz);

        unsigned char *px = generate_pixels(sz, sz, comp);

        {
            mem_buf b = encode_png(px, sz, sz, comp);
            bench_result r = run_bench_info(b.data, b.len);
            print_result_info("PNG", label, &r);
            check_result(&r, "PNG info");
            free(b.data);
        }

        {
            mem_buf b = encode_jpeg(px, sz, sz, comp, 80);
            bench_result r = run_bench_info(b.data, b.len);
            print_result_info("JPEG", label, &r);
            check_result(&r, "JPEG info");
            free(b.data);
        }

        free(px);
    }

    return 0;
}

/*
 * Suite 5: 16-bit decode throughput (PNG 16-bit)
 */
static int bench_suite_load16(void) {
    int sizes[] = { 64, 256, 512 };
    int n_sizes = g_quick ? 1 : 3;
    int comp = 3;

    printf("\n── Suite 5: 16-bit decode (stbi_load_16_from_memory, PNG) ──\n");
    print_header();

    for (int si = 0; si < n_sizes; si++) {
        int sz = sizes[si];
        char label[32];
        snprintf(label, sizeof(label), "%dx%d", sz, sz);

        unsigned char *px = generate_pixels(sz, sz, comp);
        mem_buf b = encode_png(px, sz, sz, comp);
        bench_result r = run_bench_load16(b.data, b.len, comp);
        print_result("PNG16", label, comp, &r, sz, sz, b.len);
        check_result(&r, label);
        free(b.data);
        free(px);
    }

    return 0;
}

/*
 * Suite 6: HDR decode throughput (HDR float → float decode)
 */
static int bench_suite_hdr(void) {
    int sizes[] = { 64, 256, 512 };
    int n_sizes = g_quick ? 1 : 3;
    int comp = 3;

    printf("\n── Suite 6: HDR decode (stbi_loadf_from_memory) ──\n");
    print_header();

    for (int si = 0; si < n_sizes; si++) {
        int sz = sizes[si];
        char label[32];
        snprintf(label, sizeof(label), "%dx%d", sz, sz);

        float *fpx;
        generate_pixels_float(sz, sz, comp, &fpx);
        mem_buf b = encode_hdr(fpx, sz, sz, comp);
        bench_result r = run_bench_loadf(b.data, b.len, comp);
        print_result("HDR", label, comp, &r, sz, sz, b.len);
        check_result(&r, "HDR decode");
        free(b.data);
        free(fpx);
    }

    return 0;
}

/* ── CLI ─────────────────────────────────────────────────────────────── */

static void usage(void) {
    printf("Usage: bench_stb_image [options]\n"
           "\n"
           "Options:\n"
           "  --quick         Skip larger sizes (fast smoke test)\n"
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

    printf("stb_image.h benchmark\n");
    printf("Warm-up: %d  |  Timed: auto-calibrated (%s)\n",
           g_warmup, g_quick ? "quick" : "full");
    printf("Compiler flags: -O2 -DNDEBUG\n");

    bench_suite_format();
    bench_suite_size_scaling();
    bench_suite_channels();
    bench_suite_info();
    bench_suite_load16();
    bench_suite_hdr();

    printf("\n");
    if (failures)
        printf("%d benchmark(s) had failures.\n", failures);
    else
        printf("All benchmarks completed successfully.\n");

    return failures ? 1 : 0;
}
