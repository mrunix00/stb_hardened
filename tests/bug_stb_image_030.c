// BUG-stb_image-030 Validation Test
//
// Verifies that stbi__start_mem crashes on a NULL buffer or non-positive
// length. When called from the public API stbi_load_from_memory(buffer, len,...),
// a NULL buffer with len=0 causes stbi__start_mem to set img_buffer = NULL,
// which triggers a NULL pointer dereference on the first read.
//
// Expected results:
//   Unpatched: SIGSEGV / crash (exit 139 or ASan abort)
//   Patched:   stbi_load_from_memory returns NULL gracefully (exit 0)

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    int x, y, comp;
    stbi_uc *result;

    // Trigger the bug: NULL buffer with 0 length
    result = stbi_load_from_memory(NULL, 0, &x, &y, &comp, 0);

    // If we reach this line, the fix prevented the NULL dereference
    if (result == NULL) {
        printf("BUG-030 PASS: stbi_load_from_memory(NULL, 0, ...) "
               "returned NULL gracefully\n");
        return 0;
    }

    fprintf(stderr, "BUG-030 FAIL: expected NULL return but got %p\n",
            (void *)result);
    STBI_FREE(result);
    return 1;
}
