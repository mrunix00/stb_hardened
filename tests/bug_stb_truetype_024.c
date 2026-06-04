/*
 * Regression test for BUG-stb_truetype-024:
 *   stbtt__find_table underflows the (stbtt_uint32)data_len - tabledir
 *   subtraction when data_len is smaller than the table-directory start,
 *   producing a huge num_tables and walking the table directory way past
 *   the heap buffer.
 *
 * Crash input (12 bytes total, 1-byte mode prefix + 11-byte font payload):
 *   0x07  0x00 0x01 0x00 0x00  0x00 0x00  0x00 0x00 0x00 0x0c 0x00
 *
 * The 11-byte font payload has a TrueType signature, declares
 * num_tables = 0, so the only thing that walks the table directory is
 * the (data_len > 0) cap path. Because data_len (11) < tabledir (12)
 * the unsigned subtraction wraps to ~4 billion / 16 and num_tables
 * becomes ~268 million, so the very first read at offset 12 is OOB.
 *
 * The test calls stbtt_InitFontEx with the 11-byte payload and the
 * real length. The first table lookup ("cmap") should not produce an
 * ASan fault after the fix.
 */

#include <stdio.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

static const unsigned char kFontPayload[11] = {
    0x00, 0x01, 0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00, 0x00, 0x0c, 0x00
};

int main(void)
{
    stbtt_fontinfo info;
    int rc;

    memset(&info, 0, sizeof(info));
    rc = stbtt_InitFontEx(&info, kFontPayload, 0, (int) sizeof(kFontPayload));
    (void) rc;

    return 0;
}
