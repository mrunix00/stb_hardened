/*
 * Regression test for BUG-stb_truetype-023:
 *   stbtt_FindMatchingFont walks the table directory with data_len == 0
 *   and reads past the heap buffer on tiny inputs.
 *
 * Crash input (10 bytes total, 1-byte mode prefix + 9-byte font payload):
 *   0x01 0x00 0x01 0x00 0x00 0x09 0x5b 0xa8 0xa8 0x40
 *
 * The 9-byte font payload starts with 0x00010000 which is the TrueType
 * signature (passes stbtt__isfont), declares num_tables = 0x0009, so
 * stbtt__find_table tries to walk 9 table-directory entries starting
 * at offset 12 — well past the 9-byte buffer.
 *
 * The test exercises both the "Arial" and "DejaVu" name lookups (each
 * passes non-zero STBTT_MACSTYLE_* flags so the head-table lookup path
 * is taken). Either call should not produce an ASan/UBSan fault after
 * the fix.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

/* The 10-byte font payload (no fuzzer mode prefix). The first four bytes
 * form the TrueType signature so stbtt__isfont() returns true. */
static const unsigned char kFontPayload[10] = {
    0x00, 0x01, 0x00, 0x00,
    0x00, 0x09,
    0x5b, 0xa8, 0xa8, 0x40
};

int main(void)
{
    const unsigned char *data = kFontPayload;
    int size = (int) sizeof(kFontPayload);

    (void) stbtt_FindMatchingFont(data, "Arial",  STBTT_MACSTYLE_NONE);
    (void) stbtt_FindMatchingFont(data, "DejaVu", STBTT_MACSTYLE_BOLD);

    (void) size;
    return 0;
}
