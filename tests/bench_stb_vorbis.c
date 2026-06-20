/*
 * bench_stb_vorbis.c — Performance benchmark for stb_vorbis.c
 *
 * Generates test Vorbis files via ffmpeg, then measures decode throughput
 * using the memory-based streaming API.
 *
 * Build:
 *   clang -O2 -DNDEBUG -I.. -I. tests/bench_stb_vorbis.c -lm -o build/bench_stb_vorbis
 *
 * Usage:
 *   ./build/bench_stb_vorbis [--quick] [--iters N] [--warmup N] [--help]
 *
 * Requires: ffmpeg (to generate test OGG Vorbis files at startup)
 */

#define STB_VORBIS_IMPLEMENTATION
#include "stb_vorbis.c"

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

/* ── Test data generation ────────────────────────────────────────────── */

/* Generate a test OGG Vorbis file using ffmpeg. Returns malloc'd buffer
   and sets *out_len. Caller must free. Returns NULL on failure. */
static unsigned char *generate_test_ogg(int duration_sec, int sample_rate,
                                        int channels, int *out_len) {
    char wav_cmd[256], ogg_cmd[516];
    const char *wav_path = "/tmp/bench_vorbis_test.wav";
    const char *ogg_path = "/tmp/bench_vorbis_test.ogg";

    /* Generate WAV with sine wave */
    snprintf(wav_cmd, sizeof(wav_cmd),
        "ffmpeg -y -f lavfi -i \"sine=frequency=440:duration=%d:sample_rate=%d\" "
        "-ac %d \"%s\" 2>/dev/null",
        duration_sec, sample_rate, channels, wav_path);

    /* Convert to OGG Vorbis */
    snprintf(ogg_cmd, sizeof(ogg_cmd),
        "ffmpeg -y -i \"%s\" -c:a libvorbis -q:a 5 \"%s\" 2>/dev/null",
        wav_path, ogg_path);

    if (system(wav_cmd) != 0) { fprintf(stderr, "ffmpeg WAV generation failed\n"); return NULL; }
    if (system(ogg_cmd) != 0) { fprintf(stderr, "ffmpeg OGG conversion failed\n"); return NULL; }

    /* Read the OGG file */
    FILE *f = fopen(ogg_path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    int len = (int)ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *buf = (unsigned char *)malloc((size_t)len);
    if (!buf) { fclose(f); return NULL; }
    if ((int)fread(buf, 1, (size_t)len, f) != len) { free(buf); fclose(f); return NULL; }
    fclose(f);

    *out_len = len;

    /* Clean up temp files */
    remove(wav_path);
    remove(ogg_path);

    return buf;
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
    printf("  %-32s %9s %9s %9s %10s %8s\n",
           "Operation", "Min(ms)", "Mean(ms)", "Max(ms)", "Ksamp/s", "Iters");
    printf("  %-32s %9s %9s %9s %10s %8s\n",
           "--------------------------------",
           "---------", "---------", "---------",
           "----------", "--------");
}

static void print_result(const char *label, int nsamples, bench_result *r) {
    if (!r->ok) {
        printf("  %-32s    FAILED\n", label);
        return;
    }
    double ksamp = (double)nsamples / (r->mean_ms * 1e3);
    printf("  %-32s %9.3f %9.3f %9.3f %10.1f %8d\n",
           label, r->min_ms, r->mean_ms, r->max_ms, ksamp, r->iters);
}

static int failures = 0;

static void check_result(bench_result *r, const char *label) {
    if (!r->ok) { printf("  >> FAIL: %s\n", label); failures++; }
}

/* ── Benchmark context structs ───────────────────────────────────────── */

typedef struct {
    unsigned char *data;
    int data_len;
} vorbis_mem;

typedef struct {
    vorbis_mem *vorb;
    int chunk_samples; /* samples per get_samples call */
    int channels;
} decode_ctx;

typedef struct {
    vorbis_mem *vorb;
    int channels;
} decode_frame_ctx;

typedef struct {
    vorbis_mem *vorb;
    unsigned int *seek_positions;
    int n_seeks;
} seek_ctx;

/* ── Benchmark functions ─────────────────────────────────────────────── */

/* One-shot decode entire stream via stb_vorbis_decode_memory */
static int bench_decode_memory(void *vctx) {
    decode_frame_ctx *c = (decode_frame_ctx *)vctx;
    int channels, sample_rate;
    short *output = NULL;
    int n = stb_vorbis_decode_memory(c->vorb->data, c->vorb->data_len,
                                      &channels, &sample_rate, &output);
    if (output) free(output);
    return n > 0;
}

/* Streaming decode: open + decode all frames with get_frame_short_interleaved */
static int bench_decode_stream_short(void *vctx) {
    decode_frame_ctx *c = (decode_frame_ctx *)vctx;
    int err;
    stb_vorbis *v = stb_vorbis_open_memory(c->vorb->data, c->vorb->data_len, &err, NULL);
    if (!v) return 0;

    short buf[4096];
    int total = 0;
    int n;
    while ((n = stb_vorbis_get_frame_short_interleaved(v, c->channels, buf, 4096)) > 0)
        total += n;

    stb_vorbis_close(v);
    return total > 0;
}

/* Streaming decode with get_samples_short_interleaved (chunked) */
static int bench_decode_samples_short(void *vctx) {
    decode_ctx *c = (decode_ctx *)vctx;
    int err;
    stb_vorbis *v = stb_vorbis_open_memory(c->vorb->data, c->vorb->data_len, &err, NULL);
    if (!v) return 0;

    int total_frames = c->chunk_samples * c->channels;
    short *buf = (short *)malloc((size_t)(total_frames * sizeof(short)));
    int total = 0;
    int n;
    while ((n = stb_vorbis_get_samples_short_interleaved(v, c->channels, buf, total_frames)) > 0)
        total += n;

    free(buf);
    stb_vorbis_close(v);
    return total > 0;
}

/* Streaming decode with get_samples_float_interleaved (chunked) */
static int bench_decode_samples_float(void *vctx) {
    decode_ctx *c = (decode_ctx *)vctx;
    int err;
    stb_vorbis *v = stb_vorbis_open_memory(c->vorb->data, c->vorb->data_len, &err, NULL);
    if (!v) return 0;

    int total_frames = c->chunk_samples * c->channels;
    float *buf = (float *)malloc((size_t)(total_frames * sizeof(float)));
    int total = 0;
    int n;
    while ((n = stb_vorbis_get_samples_float_interleaved(v, c->channels, buf, total_frames)) > 0)
        total += n;

    free(buf);
    stb_vorbis_close(v);
    return total > 0;
}

/* Seek to random positions */
static int bench_seek(void *vctx) {
    seek_ctx *c = (seek_ctx *)vctx;
    int err;
    stb_vorbis *v = stb_vorbis_open_memory(c->vorb->data, c->vorb->data_len, &err, NULL);
    if (!v) return 0;

    for (int i = 0; i < c->n_seeks; i++)
        stb_vorbis_seek_frame(v, c->seek_positions[i]);

    stb_vorbis_close(v);
    return 1;
}

/* ── Suites ──────────────────────────────────────────────────────────── */

static int bench_suite_oneshot(vorbis_mem *vorb, int total_samples) {
    printf("\n── Suite 1: One-shot decode (stb_vorbis_decode_memory) ──\n");
    print_header();

    decode_frame_ctx ctx = { vorb, 2 };
    bench_result r = run_bench(bench_decode_memory, &ctx);
    print_result("decode_memory (entire stream)", total_samples, &r);
    check_result(&r, "decode_memory");

    return 0;
}

static int bench_suite_streaming(vorbis_mem *vorb, int total_samples, int channels) {
    printf("\n── Suite 2: Streaming decode (frame-by-frame) ──\n");
    print_header();

    /* get_frame_short_interleaved */
    {
        decode_frame_ctx ctx = { vorb, channels };
        bench_result r = run_bench(bench_decode_stream_short, &ctx);
        print_result("get_frame_short_interleaved", total_samples, &r);
        check_result(&r, "stream_short");
    }

    return 0;
}

static int bench_suite_chunked(vorbis_mem *vorb, int total_samples, int channels) {
    int chunks[] = { 64, 256, 1024, 4096, 16384 };
    int n_chunks = g_quick ? 3 : 5;

    printf("\n── Suite 3: Chunked decode (get_samples, vary chunk size) ──\n");
    print_header();

    for (int ci = 0; ci < n_chunks; ci++) {
        int chunk = chunks[ci];
        char label[48];

        /* short interleaved */
        {
            decode_ctx ctx = { vorb, chunk, channels };
            snprintf(label, sizeof(label), "short chunk=%d", chunk);
            bench_result r = run_bench(bench_decode_samples_short, &ctx);
            print_result(label, total_samples, &r);
            check_result(&r, label);
        }

        /* float interleaved */
        {
            decode_ctx ctx = { vorb, chunk, channels };
            snprintf(label, sizeof(label), "float chunk=%d", chunk);
            bench_result r = run_bench(bench_decode_samples_float, &ctx);
            print_result(label, total_samples, &r);
            check_result(&r, label);
        }
    }

    return 0;
}

static int bench_suite_seeking(vorbis_mem *vorb, int total_samples) {
    int n_seeks = g_quick ? 50 : 200;

    printf("\n── Suite 4: Seek performance (%d random seeks) ──\n", n_seeks);
    print_header();

    /* Generate random seek positions */
    unsigned int *positions = (unsigned int *)malloc((size_t)n_seeks * sizeof(unsigned int));
    for (int i = 0; i < n_seeks; i++)
        positions[i] = (unsigned int)((uint64_t)rand() * total_samples / RAND_MAX);

    seek_ctx ctx = { vorb, positions, n_seeks };
    bench_result r = run_bench(bench_seek, &ctx);
    char label[48];
    snprintf(label, sizeof(label), "seek_frame x%d", n_seeks);
    print_result(label, n_seeks, &r);
    check_result(&r, "seeking");

    free(positions);
    return 0;
}

/* ── CLI ─────────────────────────────────────────────────────────────── */

static void usage(void) {
    printf("Usage: bench_stb_vorbis [options]\n"
           "\n"
           "Options:\n"
           "  --quick         Skip larger sizes / fewer variants (fast smoke test)\n"
           "  --iters N       Fixed iteration count (bypass auto-calibration)\n"
           "  --warmup N      Warm-up iterations (default: 3)\n"
           "  --help          Show this help\n"
           "\n"
           "Requires ffmpeg to generate test Vorbis files at startup.\n");
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

    printf("stb_vorbis.c benchmark\n");
    printf("Warm-up: %d  |  Timed: auto-calibrated (%s)\n",
           g_warmup, g_quick ? "quick" : "full");
    printf("Compiler flags: -O2 -DNDEBUG\n");

    /* Generate test data */
    printf("Generating test Vorbis data (ffmpeg)...\n");
    int ogg_len;
    int duration = g_quick ? 2 : 5;
    unsigned char *ogg_data = generate_test_ogg(duration, 44100, 2, &ogg_len);
    if (!ogg_data) {
        fprintf(stderr, "Failed to generate test Vorbis data. Is ffmpeg installed?\n");
        return 1;
    }

    /* Get stream info */
    int err;
    stb_vorbis *v = stb_vorbis_open_memory(ogg_data, ogg_len, &err, NULL);
    if (!v) {
        fprintf(stderr, "Failed to open test Vorbis data (error %d)\n", err);
        free(ogg_data);
        return 1;
    }
    stb_vorbis_info info = stb_vorbis_get_info(v);
    int total_samples = (int)stb_vorbis_stream_length_in_samples(v);
    float duration_s = stb_vorbis_stream_length_in_seconds(v);
    stb_vorbis_close(v);

    printf("  OGG size: %d bytes  |  Channels: %d  |  Rate: %d Hz\n",
           ogg_len, info.channels, info.sample_rate);
    printf("  Duration: %.1f s  |  Total samples: %d\n", duration_s, total_samples);

    vorbis_mem vorb = { ogg_data, ogg_len };

    bench_suite_oneshot(&vorb, total_samples);
    bench_suite_streaming(&vorb, total_samples, info.channels);
    bench_suite_chunked(&vorb, total_samples, info.channels);
    bench_suite_seeking(&vorb, total_samples);

    free(ogg_data);

    printf("\n");
    if (failures)
        printf("%d benchmark(s) had failures.\n", failures);
    else
        printf("All benchmarks completed successfully.\n");

    return failures ? 1 : 0;
}
