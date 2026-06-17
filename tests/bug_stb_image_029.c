// BUG-stb_image-029 Validation Test
//
// Verifies that stbi__refill_buffer mishandles a negative return value
// from io.read. If io.read returns a negative value (indicating error),
// the function computes s->img_buffer_end = s->buffer_start + n, setting
// the end before the start. Subsequent reads then return data from before
// the buffer.
//
// Expected results:
//   Unpatched: After refill_buffer with n=-1, img_buffer_end is set before
//              img_buffer_start, so reads return stack junk or stale memory.
//   Patched:   Negative return from io.read is treated as EOF or rejected.

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Custom io.read callback that returns a negative value on the first call
static int malicious_read(void *user, char *data, int size)
{
    (void)user;
    (void)data;
    (void)size;
    return -1;  // simulate read error
}

// Dummy callbacks for the other io operations
static void malicious_skip(void *user, int n) { (void)user; (void)n; }
static int malicious_eof(void *user) { (void)user; return 0; }

int main(void)
{
    stbi_io_callbacks cb;
    cb.read = malicious_read;
    cb.skip = malicious_skip;
    cb.eof  = malicious_eof;

    int x, y, comp;
    stbi_uc *result;

    // Exercise the refill_buffer path via callback-based loading
    // This will trigger stbi__refill_buffer -> io.read returns -1
    result = stbi_load_from_callbacks(&cb, NULL, &x, &y, &comp, 0);

    // If we reach here, the decoder didn't crash/ASan.
    // Expected behavior: NULL returned gracefully (negative read
    // should be treated as EOF or error).
    if (result == NULL) {
        printf("BUG-029 PASS: load_from_callbacks returned NULL "
               "(no crash from negative io.read)\n");
        return 0;
    }

    fprintf(stderr, "BUG-029 FAIL: expected NULL, got %p\n",
            (void *)result);
    STBI_FREE(result);
    return 1;
}
