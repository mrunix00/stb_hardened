/*
 * bench_stb_c_lexer.c — Performance benchmark for stb_c_lexer.h
 *
 * Generates synthetic C source code at runtime, then measures tokenization
 * throughput across different sizes and code patterns.
 *
 * Build:
 *   clang -O2 -DNDEBUG -I.. -I. tests/bench_stb_c_lexer.c -lm -o build/bench_stb_c_lexer
 *
 * Usage:
 *   ./build/bench_stb_c_lexer [--quick] [--iters N] [--warmup N] [--help]
 */

#define STB_C_LEXER_IMPLEMENTATION
#include "stb_c_lexer.h"

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

/* ── Synthetic C code generation ─────────────────────────────────────── */

static char *generate_c_code(int target_bytes, int *out_len) {
    /* Pre-built line templates — each is a complete line of C code */
    static const char *lines[] = {
        "int foo = 42;\n",
        "float bar = 3.14f;\n",
        "if (foo > baz) {\n",
        "    result += foo * bar;\n",
        "}\n",
        "for (int i = 0; i < 100; ++i) {\n",
        "    data[i] = \"hello\";\n",
        "}\n",
        "/* comment about result */\n",
        "// value 123\n",
        "return foo(42, bar);\n",
        "baz = (foo > 10) ? bar : result;\n",
        "static const char *name = \"hello world\";\n",
        "struct node { int x; float y; };\n",
        "void process(int a, int b, int c) {\n",
        "    result = a + b * c - (a << 2);\n",
        "}\n",
    };
    int n_lines = sizeof(lines) / sizeof(lines[0]);

    int cap = target_bytes + 4096;
    char *buf = (char *)malloc((size_t)cap);
    if (!buf) return NULL;
    buf[0] = '\0';

    int pos = 0;
    int idx = 0;
    while (pos < target_bytes - 100) {
        const char *line = lines[idx % n_lines];
        int ll = (int)strlen(line);
        if (pos + ll >= cap - 1) break;
        memcpy(buf + pos, line, (size_t)ll);
        pos += ll;
        idx++;
    }

    buf[pos] = '\0';
    *out_len = pos;
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
           "Operation", "Min(ms)", "Mean(ms)", "Max(ms)", "MB/s", "Iters");
    printf("  %-32s %9s %9s %9s %10s %8s\n",
           "--------------------------------",
           "---------", "---------", "---------",
           "----------", "--------");
}

static void print_result(const char *label, int nbytes, bench_result *r) {
    if (!r->ok) {
        printf("  %-32s    FAILED\n", label);
        return;
    }
    double mbps = (double)nbytes / (r->mean_ms * 1e3);
    printf("  %-32s %9.3f %9.3f %9.3f %10.1f %8d\n",
           label, r->min_ms, r->mean_ms, r->max_ms, mbps, r->iters);
}

static int failures = 0;

static void check_result(bench_result *r, const char *label) {
    if (!r->ok) { printf("  >> FAIL: %s\n", label); failures++; }
}

/* ── Benchmark context structs ───────────────────────────────────────── */

typedef struct {
    char *code;
    int code_len;
    int string_store_size;
} lex_ctx;

/* ── Benchmark functions ─────────────────────────────────────────────── */

/* Tokenize entire input, counting tokens */
static int bench_tokenize(void *vctx) {
    lex_ctx *c = (lex_ctx *)vctx;
    stb_lexer lexer;
    char store[65536];
    stb_c_lexer_init(&lexer, c->code, c->code + c->code_len, store, (int)sizeof(store));
    int count = 0;
    while (stb_c_lexer_get_token(&lexer))
        ++count;
    return count > 0;
}

/* ── Suites ──────────────────────────────────────────────────────────── */

static int bench_suite_size_scaling(void) {
    int sizes[] = { 1024, 4096, 16384, 65536, 262144 };
    int n_sizes = g_quick ? 3 : 5;

    printf("\n── Suite 1: Input size scaling ──\n");
    print_header();

    for (int si = 0; si < n_sizes; si++) {
        int sz = sizes[si];
        int code_len;
        char *code = generate_c_code(sz, &code_len);
        if (!code) { printf("  >> alloc failed for %d bytes\n", sz); failures++; continue; }

        lex_ctx ctx = { code, code_len, 65536 };
        bench_result r = run_bench(bench_tokenize, &ctx);
        char label[48];
        snprintf(label, sizeof(label), "tokenize %dKB input", code_len / 1024);
        print_result(label, code_len, &r);
        check_result(&r, label);

        free(code);
    }

    return 0;
}

static int bench_suite_string_store(void) {
    int code_len;
    char *code = generate_c_code(65536, &code_len);
    if (!code) { printf("  >> alloc failed\n"); return 1; }

    int store_sizes[] = { 256, 1024, 4096, 16384, 65536 };
    int n_sizes = g_quick ? 3 : 5;

    printf("\n── Suite 2: String store size (64KB input) ──\n");
    print_header();

    for (int si = 0; si < n_sizes; si++) {
        int ss = store_sizes[si];
        lex_ctx ctx = { code, code_len, ss };
        bench_result r = run_bench(bench_tokenize, &ctx);
        char label[48];
        snprintf(label, sizeof(label), "tokenize store=%d", ss);
        print_result(label, code_len, &r);
        check_result(&r, label);
    }

    free(code);
    return 0;
}

static int bench_suite_token_density(void) {
    /* Different code styles produce different token densities.
       Dense: short identifiers, many operators, many integers
       Sparse: long strings, lots of whitespace, fewer tokens */

    /* Dense code: lots of small expressions */
    const char *dense_code =
        "x=1;y=2;z=x+y;w=z*x-y;v=w+z;u=v*x+y;return u+w+v+x+y+z;\n"
        "a=0;b=1;c=2;d=3;e=4;f=5;g=6;h=7;i=8;j=9;\n"
        "if(a<b){c=d+e;f=g*h;i=j+k;}else{f=i-j;h=g/e;d=c*b;}\n";

    /* Sparse code: long strings and identifiers */
    const char *sparse_code =
        "static const char *long_identifier_name_here_that_is_quite_verbose = "
        "\"this is a very long string literal that takes up a lot of bytes but is only a single token\";\n"
        "    \n"
        "/* another long comment that contains many characters but produces no tokens at all */\n"
        "int short_id;\n";

    printf("\n── Suite 3: Token density comparison ──\n");
    print_header();

    /* Dense */
    {
        int len = (int)strlen(dense_code);
        /* Repeat to get enough data */
        int repeat = 2000;
        int total = len * repeat;
        char *buf = (char *)malloc((size_t)total + 1);
        buf[0] = '\0';
        for (int i = 0; i < repeat; i++)
            memcpy(buf + i * len, dense_code, (size_t)len);
        buf[total] = '\0';

        lex_ctx ctx = { buf, total, 65536 };
        bench_result r = run_bench(bench_tokenize, &ctx);
        print_result("dense (ops/ints/idents)", total, &r);
        check_result(&r, "dense");

        free(buf);
    }

    /* Sparse */
    {
        int len = (int)strlen(sparse_code);
        int repeat = 2000;
        int total = len * repeat;
        char *buf = (char *)malloc((size_t)total + 1);
        buf[0] = '\0';
        for (int i = 0; i < repeat; i++)
            memcpy(buf + i * len, sparse_code, (size_t)len);
        buf[total] = '\0';

        lex_ctx ctx = { buf, total, 65536 };
        bench_result r = run_bench(bench_tokenize, &ctx);
        print_result("sparse (strings/comments)", total, &r);
        check_result(&r, "sparse");

        free(buf);
    }

    /* Mixed: realistic C code */
    {
        int code_len;
        char *code = generate_c_code(65536, &code_len);
        if (!code) { printf("  >> alloc failed\n"); return 1; }

        lex_ctx ctx = { code, code_len, 65536 };
        bench_result r = run_bench(bench_tokenize, &ctx);
        print_result("mixed (generated C code)", code_len, &r);
        check_result(&r, "mixed");

        free(code);
    }

    return 0;
}

static int bench_suite_self_parse(void) {
    /* Parse the library's own source as a real-world stress test */
    FILE *f = fopen("stb_c_lexer.h", "rb");
    if (!f) {
        /* Try from the root dir */
        f = fopen("../stb_c_lexer.h", "rb");
        if (!f) {
            f = fopen("../../stb_c_lexer.h", "rb");
        }
    }
    if (!f) {
        printf("\n── Suite 4: Self-parse (skipped — stb_c_lexer.h not found) ──\n");
        return 0;
    }

    fseek(f, 0, SEEK_END);
    int len = (int)ftell(f);
    fseek(f, 0, SEEK_SET);
    char *code = (char *)malloc((size_t)len);
    if (!code) { fclose(f); return 1; }
    if ((int)fread(code, 1, (size_t)len, f) != len) { free(code); fclose(f); return 1; }
    fclose(f);

    printf("\n── Suite 4: Self-parse (stb_c_lexer.h, %d bytes) ──\n", len);
    print_header();

    lex_ctx ctx = { code, len, 65536 };
    bench_result r = run_bench(bench_tokenize, &ctx);
    print_result("tokenize own source", len, &r);
    check_result(&r, "self_parse");

    free(code);
    return 0;
}

/* ── CLI ─────────────────────────────────────────────────────────────── */

static void usage(void) {
    printf("Usage: bench_stb_c_lexer [options]\n"
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

    printf("stb_c_lexer.h benchmark\n");
    printf("Warm-up: %d  |  Timed: auto-calibrated (%s)\n",
           g_warmup, g_quick ? "quick" : "full");
    printf("Compiler flags: -O2 -DNDEBUG\n");

    bench_suite_size_scaling();
    bench_suite_string_store();
    bench_suite_token_density();
    bench_suite_self_parse();

    printf("\n");
    if (failures)
        printf("%d benchmark(s) had failures.\n", failures);
    else
        printf("All benchmarks completed successfully.\n");

    return failures ? 1 : 0;
}
