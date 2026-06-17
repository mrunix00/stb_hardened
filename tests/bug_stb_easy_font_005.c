#define STB_EASY_FONT_IMPLEMENTATION
#include "stb_easy_font.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    char text[] = "Hello";

    printf("Testing BUG-005: Misaligned vertex buffer float access...\n");

    /* Create a properly aligned buffer (malloc guarantees >= 16-byte alignment),
     * then offset by 1 byte to force misalignment. */
    char *aligned = (char *)malloc(128);
    if (!aligned) {
        fprintf(stderr, "malloc failed\n");
        return 1;
    }
    char *misaligned = aligned + 1;  /* guaranteed odd address */

    /* Call stb_easy_font_print with the misaligned buffer.
     * The unpatched code casts misaligned to (float *) and writes to it,
     * triggering UBSan: "store to misaligned address ... for type 'float'".
     * On most x86 systems this doesn't crash, but it's still UB that
     * UBSan reliably detects. */
    int result = stb_easy_font_print(0, 0, text, NULL, misaligned, 127);
    printf("  misaligned-buffer print returned %d\n", result);

    /* Sanity check: aligned buffer should always work */
    result = stb_easy_font_print(0, 0, text, NULL, aligned, 128);
    printf("  aligned-buffer print returned %d\n", result);

    free(aligned);

    /* If we reach here without a UBSan error, the test passes */
    printf("BUG-005: PASS (no misaligned access detected)\n");
    return 0;
}
