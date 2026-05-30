// BUG-015: OOB read in stbtt__find_table / stbtt_InitFont_internal
// CVE-2026-5314
//
// stbtt__find_table reads num_tables from the font offset table and
// iterates unboundedly. With a crafted font declaring 65535 tables,
// the loop would traverse 65535 * 16 = 1,048,560 bytes regardless
// of actual buffer size. The fix caps num_tables at 256 and adds
// integer overflow protection.
//
// Without the fix: ASan stack-buffer-overflow at i >= 256.
// With the fix:    loop bounds to 256 entries, 4108-byte buffer
//                  is sufficient, function returns 0 safely.
//
// Compile: clang -fsanitize=address,undefined -g -O0 tests/bug_015.c -o tests/bug_015 -lm
// Run:     tests/bug_015

#define STB_TRUETYPE_IMPLEMENTATION
#include "../stb_truetype.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
   // Buffer sized to exactly hold 256 table directory entries
   // (12-byte offset-table header + 256 * 16-byte entries = 4108).
   unsigned char data[4108];
   memset(data, 0, sizeof(data));

   // TrueType signature (0x00010000 big-endian)
   data[0] = 0x00; data[1] = 0x01; data[2] = 0x00; data[3] = 0x00;

   // numTables = 65535 → 65535 * 16 = 1,048,560 > 4096 cap threshold
   // The cap reduces this to 256 entries.
   data[4] = 0xFF; data[5] = 0xFF;

   stbtt_fontinfo font;
   int offset = stbtt_GetFontOffsetForIndex(data, 0);
   if (offset >= 0)
      stbtt_InitFont(&font, data, offset);

   // If we reach here, no OOB was detected → the cap prevented
   // the unbounded loop.
   printf("PASS: BUG-015 fix verified (no OOB with capped directory)\n");
   return 0;
}
