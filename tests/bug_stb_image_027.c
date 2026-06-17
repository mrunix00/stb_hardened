// BUG-stb_image-027 Validation Test
//
// Verifies that stbi__getn with a negative n causes a buffer over-read.
// The function accepts int n and passes it to memcpy as size_t.
// A negative n (e.g., -1) becomes a huge positive size_t (e.g., 0xFFFFFFFFFFFFFFFF
// on 64-bit), causing memcpy to over-read far past the source buffer.
//
// Expected results:
//   Unpatched: ASan aborts with negative-size-param error
//   Patched:   stbi__getn returns 0 (reject negative n), exit 0

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    // Set up a minimal memory context
    stbi_uc src_buf[16];
    memset(src_buf, 0xAB, sizeof(src_buf));

    stbi__context s;
    stbi__start_mem(&s, src_buf, sizeof(src_buf));

    // Consume 1 byte so img_buffer is at least 1 past the buffer start,
    // preventing pointer-arithmetic underflow in the s->img_buffer+n check.
    (void)stbi__get8(&s);

    // Destination buffer -- small enough that ASan will catch the overrun
    stbi_uc dest[32];
    memset(dest, 0xCC, sizeof(dest));

    // Trigger the bug: stbi__getn with n = -1
    // Without the fix: memcpy(dest, s->img_buffer, (size_t)-1) -- ASan abort
    // With the fix:    returns 0 at the new n < 0 guard
    int result = stbi__getn(&s, dest, -1);

    // If we reach this line, ASan/UBSan did not abort.
    if (result == 0) {
        printf("BUG-027 PASS: stbi__getn correctly rejected n=-1\n");
        return 0;
    }

    fprintf(stderr,
        "BUG-027 FAIL: stbi__getn returned %d (expected 0)\n", result);
    return 1;
}
