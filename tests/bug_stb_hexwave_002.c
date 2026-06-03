#define STB_HEXWAVE_IMPLEMENTATION
#include "stb_hexwave.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main()
{
    // BUG-STB_HEXWAVE-002: hexwave_shutdown has inverted condition.
    // When user_buffer is provided, blep/blamp point INTO the user buffer,
    // but shutdown tries to free() them (invalid free).
    // Test with a user-provided buffer.
    int width = 32;
    int oversample = 16;
    // buffer size per comment: 16*width*(oversample+1)
    size_t buf_size = 16 * width * (oversample + 1);
    float *buf = (float *)malloc(buf_size);
    if (!buf) {
        fprintf(stderr, "FAIL: malloc failed\n");
        return 1;
    }
    memset(buf, 0, buf_size);

    printf("Calling hexwave_init with user_buffer...\n");
    hexwave_init(width, oversample, buf);

    printf("Calling hexwave_shutdown with user_buffer (should trigger invalid free)...\n");
    // This attempts to free() pointers inside buf, which is UB
    hexwave_shutdown(buf);

    printf("Survived shutdown (test compiled without ASan or invalid free not detected)\n");
    free(buf);
    return 0;
}
