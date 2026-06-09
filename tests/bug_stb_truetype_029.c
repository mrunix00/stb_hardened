#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STBTT_assert(x) assert(x)
#define STB_TRUETYPE_IMPLEMENTATION
#include "../stb_truetype.h"

/*
 * Regression test for BUG-stb_truetype-029.
 *
 * stbtt__cff_index_get (stb_truetype.h:1271) is declared to return
 * stbtt__buf but the success path at lines 1279-1281 reads `start`
 * and `end` from the CFF INDEX offset array and then falls through
 * the closing brace without a return statement. This is undefined
 * behavior (C11 6.9.1/12). The function is called during CFF font
 * init at line 1516 to parse the top dict, and subsequently for
 * charstring and subroutine access.
 *
 * We build a minimal in-memory CFF INDEX with known data and invoke
 * stbtt__cff_index_get directly. Without the fix, the returned
 * stbtt__buf has undefined data/cursor/size. With the fix, the
 * returned buf correctly points to the indexed entry.
 */

int main(void) {
   /* Build a minimal CFF INDEX with 1 entry of 3 bytes ("ABC"). */
   unsigned char idx[] = {
      0x00, 0x01,  /* count = 1 */
      0x01,        /* offsize = 1 */
      0x01,        /* offset[0] = 1 (1-based, points to first data byte) */
      0x04,        /* offset[1] = 4 (1 + 3 data bytes) */
      'A', 'B', 'C'
   };
   int idx_bytes = sizeof(idx);

   /* Create a stbtt__buf wrapping the raw INDEX data. */
   stbtt__buf b;
   b.data = idx;
   b.cursor = 0;
   b.size = idx_bytes;

   /* Fetch the first (only) entry. */
   stbtt__buf result = stbtt__cff_index_get(b, 0);

   /*
    * Expected: result should point to the "ABC" data with size 3.
    *
    * The entry data in a CFF INDEX starts at
    *   base = 2 + (count+1)*offsize = 2 + 2*1 = 4 bytes
    * from the start of the INDEX. Since offsets are 1-based, the
    * actual data offset is base + start - 1 = 4 + 1 - 1 = 4.
    *
    * If the missing return produces garbage, result.size will NOT
    * be 3, and/or result.data will NOT point into idx.
    */
   if (result.size != 3) {
      fprintf(stderr, "FAIL: result.size=%d, expected 3\n", result.size);
      return 1;
   }
   if (result.data != idx + 5) {
      fprintf(stderr, "FAIL: result.data points to wrong location (ptr offset %td, expected +5)\n", result.data - idx);
      return 1;
   }
   if (result.data[0] != 'A' || result.data[1] != 'B' || result.data[2] != 'C') {
      fprintf(stderr, "FAIL: result.data content mismatch\n");
      return 1;
   }
   if (result.cursor != 0) {
      fprintf(stderr, "FAIL: result.cursor=%d, expected 0\n", result.cursor);
      return 1;
   }

   printf("PASS: stbtt__cff_index_get returned correct entry\n");
   return 0;
}
