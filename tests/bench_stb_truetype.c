/*
 * bench_stb_truetype.c — Performance benchmark for stb_truetype.h
 *
 * Measures glyph metrics, rasterization, SDF, and atlas packing throughput.
 * Requires a TTF font file (defaults to system DejaVuSans.ttf).
 *
 * Build:
 *   clang -O2 -DNDEBUG -I.. -I. tests/bench_stb_truetype.c -lm -o build/bench_stb_truetype
 *
 * Usage:
 *   ./build/bench_stb_truetype [--font PATH] [--quick] [--iters N] [--warmup N] [--help]
 */

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

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
static const char *g_font_path = "/usr/share/fonts/TTF/DejaVuSans.ttf";

/* ── Font context ────────────────────────────────────────────────────── */

typedef struct {
    stbtt_fontinfo info;
    unsigned char *data;
    int data_len;
} font_ctx;

/* Defined-codepoint set (ASCII 32–126, only glyphs present in font) */
typedef struct {
    int codepoints[256];
    int count;
} cp_set;

static int font_ctx_init(font_ctx *fc, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "Cannot open font: %s\n", path); return 0; }
    fseek(f, 0, SEEK_END);
    fc->data_len = (int)ftell(f);
    fseek(f, 0, SEEK_SET);
    fc->data = (unsigned char *)malloc((size_t)fc->data_len);
    if (!fc->data) { fclose(f); return 0; }
    if ((int)fread(fc->data, 1, (size_t)fc->data_len, f) != fc->data_len) {
        fclose(f); free(fc->data); return 0;
    }
    fclose(f);

    int offset = stbtt_GetFontOffsetForIndex(fc->data, 0);
    if (offset < 0) { free(fc->data); return 0; }
    if (!stbtt_InitFont(&fc->info, fc->data, offset)) {
        free(fc->data); return 0;
    }
    return 1;
}

static void font_ctx_free(font_ctx *fc) {
    free(fc->data);
}

static void probe_codepoints(const stbtt_fontinfo *info, cp_set *out) {
    out->count = 0;
    for (int cp = 32; cp < 127; cp++) {
        int g = stbtt_FindGlyphIndex(info, cp);
        if (g != 0)
            out->codepoints[out->count++] = cp;
    }
}

/* Safe scale: falls back to ScaleForMappingEmToPixels if ascent==descent */
static float safe_scale(const stbtt_fontinfo *info, float pixels) {
    int ascent, descent;
    stbtt_GetFontVMetrics(info, &ascent, &descent, NULL);
    if (ascent == descent)
        return stbtt_ScaleForMappingEmToPixels(info, pixels);
    return stbtt_ScaleForPixelHeight(info, pixels);
}

/* ── Benchmark core ──────────────────────────────────────────────────── */

typedef struct {
    double min_ms;
    double mean_ms;
    double max_ms;
    int    iters;
    int    batch_size; /* items per iteration (for glyphs/s calc) */
    int    ok;
} bench_result;

/* Run a batch benchmark: fn(ctx) is called once per timed iteration.
   batch_size is the number of items processed per call. */
typedef int (*bench_fn)(void *ctx);

static bench_result run_bench(bench_fn fn, void *ctx, int batch_size) {
    bench_result r = {0};
    r.batch_size = batch_size;
    int calib = 5;

    /* calibration */
    uint64_t t0 = now_ns();
    for (int i = 0; i < calib; i++) {
        if (!fn(ctx)) { r.ok = 0; return r; }
    }
    uint64_t t1 = now_ns();
    uint64_t per_run = (t1 - t0) / (uint64_t)calib;

    int iterations;
    if (g_fixed_iters > 0) {
        iterations = g_fixed_iters;
    } else {
        iterations = (per_run > 0) ? (int)(500000000ULL / per_run) : 500;
        if (iterations < 5) iterations = 5;
        if (iterations > 500) iterations = 500;
    }

    /* warm-up */
    for (int i = 0; i < g_warmup; i++) {
        if (!fn(ctx)) { r.ok = 0; return r; }
    }

    /* timed run (total for mean) */
    t0 = now_ns();
    for (int i = 0; i < iterations; i++)
        fn(ctx);
    t1 = now_ns();
    r.mean_ms = (double)(t1 - t0) / iterations / 1e6;
    r.iters = iterations;

    /* per-iteration min/max (short pass) */
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

static void print_header_batch(void) {
    printf("  %-28s %9s %9s %9s %12s %8s\n",
           "Operation", "Min(ms)", "Mean(ms)", "Max(ms)", "Glyphs/s", "Iters");
    printf("  %-28s %9s %9s %9s %12s %8s\n",
           "----------------------------", "---------", "---------", "---------",
           "------------", "--------");
}

static void print_result_batch(const char *label, bench_result *r) {
    if (!r->ok) {
        printf("  %-28s    FAILED\n", label);
        return;
    }
    double glyphs_per_sec = (double)r->batch_size / (r->mean_ms / 1e3);
    printf("  %-28s %9.3f %9.3f %9.3f %12.0f %8d\n",
           label, r->min_ms, r->mean_ms, r->max_ms, glyphs_per_sec, r->iters);
}

static void print_header_persize(void) {
    printf("  %-12s %9s %9s %9s %12s %8s\n",
           "Size(px)", "Min(ms)", "Mean(ms)", "Max(ms)", "Glyphs/s", "Iters");
    printf("  %-12s %9s %9s %9s %12s %8s\n",
           "------------", "---------", "---------", "---------",
           "------------", "--------");
}

static void print_result_persize(const char *label, bench_result *r) {
    if (!r->ok) {
        printf("  %-12s    FAILED\n", label);
        return;
    }
    double glyphs_per_sec = (double)r->batch_size / (r->mean_ms / 1e3);
    printf("  %-12s %9.3f %9.3f %9.3f %12.0f %8d\n",
           label, r->min_ms, r->mean_ms, r->max_ms, glyphs_per_sec, r->iters);
}

/* ── Benchmark context structs ───────────────────────────────────────── */

typedef struct {
    font_ctx *fc;
    cp_set *cps;
} metrics_ctx;

typedef struct {
    font_ctx *fc;
    cp_set *cps;
    int pixel_height;
    unsigned char *bitmap_buf; /* pre-allocated 256x256 */
} bitmap_ctx;

typedef struct {
    font_ctx *fc;
    int *sdf_cps;
    int sdf_count;
    float scale;
    int padding;
} sdf_ctx;

/* ── Benchmark functions ─────────────────────────────────────────────── */

/* Suite 1: InitFont */
typedef struct { font_ctx *fc; } init_ctx;

static int bench_initfont(void *vctx) {
    init_ctx *ctx = (init_ctx *)vctx;
    stbtt_fontinfo info;
    int offset = stbtt_GetFontOffsetForIndex(ctx->fc->data, 0);
    if (!stbtt_InitFont(&info, ctx->fc->data, offset))
        return 0;
    return 1;
}

/* Suite 2: FindGlyphIndex */
static int bench_find_glyph(void *vctx) {
    metrics_ctx *ctx = (metrics_ctx *)vctx;
    for (int i = 0; i < ctx->cps->count; i++)
        stbtt_FindGlyphIndex(&ctx->fc->info, ctx->cps->codepoints[i]);
    return 1;
}

/* Suite 3: GetCodepointHMetrics */
static int bench_hmetrics(void *vctx) {
    metrics_ctx *ctx = (metrics_ctx *)vctx;
    int aw, lsb;
    for (int i = 0; i < ctx->cps->count; i++)
        stbtt_GetCodepointHMetrics(&ctx->fc->info, ctx->cps->codepoints[i], &aw, &lsb);
    return 1;
}

/* Suite 4: GetCodepointBox */
static int bench_cpbox(void *vctx) {
    metrics_ctx *ctx = (metrics_ctx *)vctx;
    int x0, y0, x1, y1;
    for (int i = 0; i < ctx->cps->count; i++)
        stbtt_GetCodepointBox(&ctx->fc->info, ctx->cps->codepoints[i], &x0, &y0, &x1, &y1);
    return 1;
}

/* Suite 5: GetCodepointKernAdvance */
static int bench_kern(void *vctx) {
    metrics_ctx *ctx = (metrics_ctx *)vctx;
    int n = ctx->cps->count;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            stbtt_GetCodepointKernAdvance(&ctx->fc->info,
                ctx->cps->codepoints[i], ctx->cps->codepoints[j]);
    return 1;
}

/* Suite 6: GetCodepointShape */
static int bench_shape(void *vctx) {
    metrics_ctx *ctx = (metrics_ctx *)vctx;
    for (int i = 0; i < ctx->cps->count; i++) {
        stbtt_vertex *verts = NULL;
        stbtt_GetCodepointShape(&ctx->fc->info, ctx->cps->codepoints[i], &verts);
        if (verts) STBTT_free(verts, ctx->fc->info.userdata);
    }
    return 1;
}

/* Suite 7: MakeCodepointBitmap (per-size) */
static int bench_bitmap(void *vctx) {
    bitmap_ctx *ctx = (bitmap_ctx *)vctx;
    float scale = safe_scale(&ctx->fc->info, (float)ctx->pixel_height);
    for (int i = 0; i < ctx->cps->count; i++) {
        int x0, y0, x1, y1;
        stbtt_GetCodepointBitmapBox(&ctx->fc->info, ctx->cps->codepoints[i],
                                     scale, scale, &x0, &y0, &x1, &y1);
        int w = x1 - x0;
        int h = y1 - y0;
        if (w > 0 && h > 0 && w <= 256 && h <= 256) {
            stbtt_MakeCodepointBitmap(&ctx->fc->info, ctx->bitmap_buf,
                                       w, h, w, scale, scale,
                                       ctx->cps->codepoints[i]);
        }
    }
    return 1;
}

/* Suite 8: GetCodepointSDF (pre-filtered) */
static int bench_sdf(void *vctx) {
    sdf_ctx *ctx = (sdf_ctx *)vctx;
    for (int i = 0; i < ctx->sdf_count; i++) {
        int w, h, xoff, yoff;
        unsigned char *sdf = stbtt_GetCodepointSDF(&ctx->fc->info, ctx->scale,
            ctx->sdf_cps[i], ctx->padding, 128, 3.0f, &w, &h, &xoff, &yoff);
        if (sdf) stbtt_FreeSDF(sdf, NULL);
    }
    return 1;
}

/* Suite 9: PackFontRange (full atlas pack) */
typedef struct {
    font_ctx *fc;
    float font_size;
    int first_char;   /* first valid char in contiguous range */
    int num_chars;    /* number of valid chars */
    int atlas_size;   /* atlas width = height */
} pack_ctx;

static int bench_pack(void *vctx) {
    pack_ctx *ctx = (pack_ctx *)vctx;
    int aw = ctx->atlas_size, ah = ctx->atlas_size;
    unsigned char *atlas = (unsigned char *)calloc(1, (size_t)(aw * ah));
    if (!atlas) return 0;

    stbtt_pack_context pc;
    if (!stbtt_PackBegin(&pc, atlas, aw, ah, 0, 1, NULL)) {
        free(atlas);
        return 0;
    }
    stbtt_packedchar *pdata = (stbtt_packedchar *)calloc((size_t)ctx->num_chars, sizeof(stbtt_packedchar));
    int ok = stbtt_PackFontRange(&pc, ctx->fc->data, 0,
        ctx->font_size, ctx->first_char, ctx->num_chars, pdata);
    stbtt_PackEnd(&pc);
    free(pdata);
    free(atlas);
    return ok;
}

/* Find the longest contiguous range of valid glyphs starting at or after start_cp */
static void find_valid_pack_range(font_ctx *fc, int start_cp, int *out_first, int *out_count) {
    int best_first = -1, best_count = 0;
    int cur_first = -1, cur_count = 0;
    for (int cp = start_cp; cp < 127; cp++) {
        int g = stbtt_FindGlyphIndex(&fc->info, cp);
        if (g == 0) { cur_first = -1; cur_count = 0; continue; }
        int x0, y0, x1, y1;
        stbtt_GetGlyphBox(&fc->info, g, &x0, &y0, &x1, &y1);
        if (x0 > x1 || y0 > y1) { cur_first = -1; cur_count = 0; continue; }
        if (cur_first < 0) cur_first = cp;
        cur_count++;
        if (cur_count > best_count) { best_first = cur_first; best_count = cur_count; }
    }
    *out_first = best_first;
    *out_count = best_count;
}

/* ── Failures counter ────────────────────────────────────────────────── */

static int failures = 0;

static void check_result(bench_result *r, const char *label) {
    if (!r->ok) {
        printf("  >> FAIL: %s\n", label);
        failures++;
    }
}

/* ── Suites ──────────────────────────────────────────────────────────── */

static int bench_suite_init(font_ctx *fc) {
    printf("\n── Suite 1: InitFont ──\n");
    print_header_batch();

    init_ctx ctx = { fc };
    bench_result r = run_bench(bench_initfont, &ctx, 1);
    print_result_batch("InitFont (per-call)", &r);
    check_result(&r, "InitFont");
    return 0;
}

static int bench_suite_metrics(font_ctx *fc, cp_set *cps) {
    printf("\n── Suite 2: Glyph metrics (%d glyphs/batch) ──\n", cps->count);
    print_header_batch();

    metrics_ctx ctx = { fc, cps };
    bench_result r;

    r = run_bench(bench_find_glyph, &ctx, cps->count);
    print_result_batch("FindGlyphIndex", &r);
    check_result(&r, "FindGlyphIndex");

    r = run_bench(bench_hmetrics, &ctx, cps->count);
    print_result_batch("GetCodepointHMetrics", &r);
    check_result(&r, "GetCodepointHMetrics");

    r = run_bench(bench_cpbox, &ctx, cps->count);
    print_result_batch("GetCodepointBox", &r);
    check_result(&r, "GetCodepointBox");

    r = run_bench(bench_kern, &ctx, cps->count * cps->count);
    print_result_batch("GetCodepointKernAdvance (pairs)", &r);
    check_result(&r, "GetCodepointKernAdvance");

    r = run_bench(bench_shape, &ctx, cps->count);
    print_result_batch("GetCodepointShape", &r);
    check_result(&r, "GetCodepointShape");

    return 0;
}

static int bench_suite_bitmap(font_ctx *fc, cp_set *cps) {
    int sizes[] = { 8, 12, 16, 24, 32, 48, 64, 96, 128 };
    int n_sizes = g_quick ? 4 : 9;

    printf("\n── Suite 3: MakeCodepointBitmap (%d glyphs) ──\n", cps->count);
    print_header_persize();

    unsigned char *bitmap_buf = (unsigned char *)calloc(1, 256 * 256);

    for (int si = 0; si < n_sizes; si++) {
        int px = sizes[si];
        char label[32];
        snprintf(label, sizeof(label), "%dpx", px);

        bitmap_ctx ctx = { fc, cps, px, bitmap_buf };
        bench_result r = run_bench(bench_bitmap, &ctx, cps->count);
        print_result_persize(label, &r);
        check_result(&r, label);
    }

    free(bitmap_buf);
    return 0;
}

static int bench_suite_sdf(font_ctx *fc, cp_set *cps) {
    printf("\n── Suite 4: GetCodepointSDF ──\n");

    /* Build SDF candidate set: non-empty glyphs with <=500 vertices */
    int sdf_cps[256];
    int sdf_count = 0;
    for (int i = 0; i < cps->count; i++) {
        int g = stbtt_FindGlyphIndex(&fc->info, cps->codepoints[i]);
        if (g == 0 || stbtt_IsGlyphEmpty(&fc->info, g))
            continue;
        stbtt_vertex *verts = NULL;
        int nv = stbtt_GetGlyphShape(&fc->info, g, &verts);
        if (verts) {
            if (nv <= 500)
                sdf_cps[sdf_count++] = cps->codepoints[i];
            STBTT_free(verts, fc->info.userdata);
        }
    }
    if (sdf_count == 0) {
        printf("  No suitable glyphs for SDF (all empty or too many vertices)\n");
        return 0;
    }
    printf("  SDF candidates: %d/%d glyphs\n", sdf_count, cps->count);

    int scales[] = { 8, 16, 32, 64 };
    int n_scales = g_quick ? 2 : 4;

    print_header_persize();
    for (int si = 0; si < n_scales; si++) {
        int px = scales[si];
        char label[32];
        snprintf(label, sizeof(label), "%dpx", px);

        float scale = safe_scale(&fc->info, (float)px);
        sdf_ctx ctx = { fc, sdf_cps, sdf_count, scale, 3 };
        bench_result r = run_bench(bench_sdf, &ctx, sdf_count);
        print_result_persize(label, &r);
        check_result(&r, label);
    }

    return 0;
}

static int bench_suite_pack(font_ctx *fc) {
    printf("\n── Suite 5: PackFontRange (atlas pack) ──\n");
    printf("  %-12s %4s %9s %9s %9s %8s\n",
           "FontSize", "Chars", "Min(ms)", "Mean(ms)", "Max(ms)", "Iters");
    printf("  %-12s %4s %9s %9s %9s %8s\n",
           "------------", "----", "---------", "---------", "---------", "--------");

    /* Find longest contiguous valid glyph range (skip corrupted glyphs) */
    int first_char, num_chars;
    find_valid_pack_range(fc, 33, &first_char, &num_chars);
    if (num_chars == 0) {
        printf("  No valid contiguous glyph range found for packing.\n");
        return 0;
    }
    printf("  Using valid range: cp %d-%d (%d chars)\n",
           first_char, first_char + num_chars - 1, num_chars);

    float font_sizes[] = { 12.0f, 16.0f, 24.0f, 32.0f, 48.0f };
    int n_sizes = g_quick ? 3 : 5;

    for (int si = 0; si < n_sizes; si++) {
        float fs = font_sizes[si];
        char label[32];
        snprintf(label, sizeof(label), "%.0fpx", fs);

        /* Scale atlas with font size: bigger fonts need more atlas space */
        int atlas_size = 1024;
        if (fs > 32.0f) atlas_size = 2048;
        if (fs > 48.0f) atlas_size = 4096;

        pack_ctx ctx = { fc, fs, first_char, num_chars, atlas_size };
        bench_result r = run_bench(bench_pack, &ctx, 1);
        if (!r.ok) {
            printf("  %-12s %4d    FAILED\n", label, num_chars);
            failures++;
        } else {
            printf("  %-12s %4d %9.3f %9.3f %9.3f %8d\n",
                   label, num_chars, r.min_ms, r.mean_ms, r.max_ms, r.iters);
        }
    }

    return 0;
}

/* ── Font info display ───────────────────────────────────────────────── */

static void print_font_info(font_ctx *fc) {
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&fc->info, &ascent, &descent, &lineGap);
    int x0, y0, x1, y1;
    stbtt_GetFontBoundingBox(&fc->info, &x0, &y0, &x1, &y1);
    printf("  Font: %s\n", g_font_path);
    printf("  ascent=%d descent=%d lineGap=%d\n", ascent, descent, lineGap);
    printf("  bbox=(%d,%d)-(%d,%d) numGlyphs=%d\n",
           x0, y0, x1, y1, fc->info.numGlyphs);
    printf("  File size: %d bytes\n", fc->data_len);
}

/* ── CLI ─────────────────────────────────────────────────────────────── */

static void usage(void) {
    printf("Usage: bench_stb_truetype [options]\n"
           "\n"
           "Options:\n"
           "  --font PATH     TTF font file (default: /usr/share/fonts/TTF/DejaVuSans.ttf)\n"
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
        } else if (strcmp(argv[i], "--font") == 0 && i + 1 < argc) {
            g_font_path = argv[++i];
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

    font_ctx fc;
    if (!font_ctx_init(&fc, g_font_path))
        return 1;

    cp_set cps;
    probe_codepoints(&fc.info, &cps);

    printf("stb_truetype.h benchmark\n");
    printf("Warm-up: %d  |  Timed: auto-calibrated (%s)\n",
           g_warmup, g_quick ? "quick" : "full");
    printf("Compiler flags: -O2 -DNDEBUG\n");
    print_font_info(&fc);
    printf("  Probed %d defined codepoints (ASCII 32-126)\n", cps.count);

    bench_suite_init(&fc);
    bench_suite_metrics(&fc, &cps);
    bench_suite_bitmap(&fc, &cps);
    bench_suite_sdf(&fc, &cps);
    bench_suite_pack(&fc);

    printf("\n");
    if (failures)
        printf("%d benchmark(s) had failures.\n", failures);
    else
        printf("All benchmarks completed successfully.\n");

    font_ctx_free(&fc);
    return failures ? 1 : 0;
}
