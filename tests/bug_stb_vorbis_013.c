#define STB_VORBIS_IMPLEMENTATION
#define STB_VORBIS_NO_STDIO
#include "stb_vorbis.c"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

// We need to patch the code temporarily to detect the bug.
// The bug: effective is too large by 2*c_inter when the clamping triggers.
// We'll add a runtime check by replacing line 1899 with a version that
// also validates the result.

// Instead, let's just run the decoder with ASan and various inputs to see
// if we can trigger an OOB write via the wrong clamping.

// Strategy: create a valid Vorbis stream with residue type 2, ch >= 2,
// codebook dimensions > 1, and enough data to trigger the clamping boundary.

// We'll use the poc_ogg from bug_stb_vorbis_001 as base (valid ID header)
// and construct a setup header that uses residue type 2.

// Actually, the simplest approach: compile with -fsanitize=address and
// feed various corrupted Vorbis files to see if we can trigger the bug.

// For now, let's verify the bug analytically and create a targeted test.
// The key insight: the clamping formula at line 1899 gives effective too large.
// If we can make the code write 2*c_inter elements past the intended end of
// the residue region, ASan should detect it.

// The residue region is within the IMDCT output buffer, which is part of
// the channel buffer allocated as blocksize_1 floats. The IMDCT writes to
// indices 0..blocksize_1/2-1. The "second half" (indices blocksize_1/2..blocksize_1-1)
// is used as scratch by IMDCT. If the residue decode writes into the second half
// when it shouldn't, IMDCT will produce wrong results but may not crash.

// However, if the over-write goes past blocksize_1, ASan will catch it.

// Let's verify: the clamping limits effective to len*ch - current_pos.
// With the bug, effective = len*ch - p_inter*ch + c_inter.
// The loop writes effective elements starting at (c_inter, p_inter).
// After writing, p_inter increases by effective/ch and c_inter wraps.
// The last element written is at position current_pos + effective - 1.
// With the bug: current_pos + (len*ch - current_pos + 2*c_inter) - 1
//            = len*ch + 2*c_inter - 1
// So it writes up to index len*ch + 2*c_inter - 1, which is 2*c_inter past the end.

// For ASan to catch this, the buffer must be exactly len*ch elements.
// The channel buffer is allocated as blocksize_1 floats, and len = blocksize_1/2.
// So the buffer has 2*len elements per channel, total 2*len*ch elements.
// The over-write goes to len*ch + 2*c_inter - 1, which is within 2*len*ch,
// so ASan won't catch it (it's within the allocation).

// However, the IMDCT uses the first half (0..len-1) and second half (len..2*len-1).
// Writing to the second half corrupts IMDCT scratch space, producing wrong audio.
// This is a logic bug, not a memory safety bug per se.

// Let's just confirm the bug analytically and create a minimal reproduction
// that demonstrates the incorrect effective value.

int main(void) {
    printf("BUG-stb_vorbis-013: verifying clamping sign error analytically\n");
    printf("\n");
    printf("The bug is at stb_vorbis.c:1899:\n");
    printf("  effective = len*ch - (p_inter*ch - c_inter);\n");
    printf("\n");
    printf("Correct formula should be:\n");
    printf("  effective = len*ch - (p_inter*ch + c_inter);\n");
    printf("\n");

    // Demonstrate the off-by-error with concrete numbers
    int ch = 2;
    int len = 256;
    int p_inter = 255;  // near the end
    int c_inter = 1;    // mid-channel

    int current_pos = p_inter * ch + c_inter;  // 511
    int total = len * ch;                       // 512
    int remaining = total - current_pos;         // 1 (correct)

    int effective_buggy = len*ch - (p_inter*ch - c_inter);  // 512 - (510 - 1) = 3
    int effective_fixed = len*ch - (p_inter*ch + c_inter);  // 512 - (510 + 1) = 1

    printf("Example: ch=%d, len=%d, p_inter=%d, c_inter=%d\n", ch, len, p_inter, c_inter);
    printf("  current_pos = %d, total = %d\n", current_pos, total);
    printf("  correct remaining = %d\n", remaining);
    printf("  buggy effective   = %d (too large by %d)\n", effective_buggy, 2*c_inter);
    printf("  fixed effective   = %d\n", effective_fixed);
    printf("\n");

    if (effective_buggy != remaining) {
        printf("BUG CONFIRMED: buggy effective (%d) != correct remaining (%d)\n",
               effective_buggy, remaining);
        printf("The over-write is %d elements past the intended boundary.\n",
               effective_buggy - remaining);
    } else {
        printf("Bug not triggered with these parameters.\n");
    }

    return 0;
}
