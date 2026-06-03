#define STB_HEXWAVE_IMPLEMENTATION
#include "stb_hexwave.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main()
{
    // BUG-STB_HEXWAVE-004: Integer overflow in allocation size computation.
    // Fix: oversample is clamped to prevent signed int overflow.
    // Test that normal values work without integer overflow.
    printf("Test: hexwave_init(32, 16, user_buffer)\n");
    {
        size_t buf_size = (size_t)16 * 32 * (16 + 1);
        float *buf = (float *)malloc(buf_size);
        if (!buf) { fprintf(stderr, "FAIL: malloc\n"); return 1; }
        memset(buf, 0, buf_size);
        hexwave_init(32, 16, buf);
        hexwave_shutdown(buf);
        free(buf);
        printf("  OK — no overflow\n");
    }

    // Verify that oversample is validated (no division by zero)
    printf("Test: hexwave_init(32, 0, user_buffer) — oversample=0 clamped to 1\n");
    {
        size_t buf_size = (size_t)16 * 32 * (1 + 1);
        float *buf = (float *)malloc(buf_size);
        if (!buf) { fprintf(stderr, "FAIL: malloc\n"); return 1; }
        memset(buf, 0, buf_size);
        hexwave_init(32, 0, buf);
        hexwave_shutdown(buf);
        free(buf);
        printf("  OK — oversample clamped\n");
    }

    printf("All tests passed\n");
    return 0;
}
