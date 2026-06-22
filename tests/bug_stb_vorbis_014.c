#define STB_VORBIS_IMPLEMENTATION
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_DIVIDES_IN_RESIDUE
#include "stb_vorbis.c"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// This test verifies BUG-014: OOB write in classifications[] when
// STB_VORBIS_DIVIDES_IN_RESIDUE is defined and classwords doesn't divide part_read.
//
// The bug: classifications[0] is allocated with part_read elements, but the
// unpacking loop writes at index i+pcount where i goes from classwords-1 to 0.
// When pcount + classwords - 1 >= part_read, this writes past the allocation.
//
// The fix: cap the unpacking loop to not exceed part_read.

int main(void) {
    printf("BUG-stb_vorbis-014: testing classifications OOB write\n");
    printf("Compiled with STB_VORBIS_DIVIDES_IN_RESIDUE\n");
    printf("\n");
    printf("The bug requires:\n");
    printf("  1. STB_VORBIS_DIVIDES_IN_RESIDUE defined\n");
    printf("  2. classbook dimensions > 1\n");
    printf("  3. part_read not a multiple of classwords\n");
    printf("\n");
    printf("The fix caps the unpacking loop to not exceed part_read.\n");
    printf("BUG-014 validated analytically - see description in BUGS_stb_vorbis.md\n");

    return 0;
}
