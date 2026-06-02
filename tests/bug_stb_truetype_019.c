#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "../stb_truetype.h"

/*
 * Regression test for BUG-stb_truetype-019.
 *
 * The internal helpers ttULONG / ttLONG / ttSHORT read multi-byte
 * integers from the font buffer using expressions of the form
 * (p[0]<<24) + (p[1]<<16) + (p[2]<<8) + p[3]. p[i] is an unsigned
 * char that integer-promotes to a signed int; when the high bit of
 * p[0] (or p[1] when shifted by 16) is set, the left shift produces
 * a negative int, which is undefined behavior in C. UBSan flags it
 * as `left shift of 255 by 24 places cannot be represented in type
 * 'int'`.
 *
 * The buffer must be initialized at runtime (not as a static
 * initializer), otherwise the compiler can constant-fold the call
 * to ttULONG inside the inlined stbtt__find_table and UBSan never
 * sees the shift.
 *
 * Unpatched: UBSan halts with `runtime error: left shift of 255 by
 * 24 places cannot be represented in type 'int'` in ttULONG called
 * from stbtt__find_table inside stbtt_InitFont_internal.
 * Patched:   the cast to stbtt_uint32 silences the UBSan trap; the
 * bogus offset is then rejected by the BUG-014 bounds check.
 */

int main(void) {
   uint8_t *font = (uint8_t *)malloc(32);
   if (!font) return 2;
   font[0]=0x00; font[1]=0x01; font[2]=0x00; font[3]=0x00;       /* sfVersion */
   font[4]=0x00; font[5]=0x01;                                   /* numTables = 1 */
   font[6]=0x00; font[7]=0x10; font[8]=0x00; font[9]=0x00;      /* searchRange etc. */
   font[10]=0x00; font[11]=0x10; font[12]='c'; font[13]='m';
   font[14]='a'; font[15]='p';
   font[16]=0x00; font[17]=0x00; font[18]=0x00; font[19]=0x00;  /* checkSum */
   font[20]=0xFF; font[21]=0x00; font[22]=0x00; font[23]=0x00;  /* offset = 0xFF000000 */
   font[24]=0x00; font[25]=0x00; font[26]=0x00; font[27]=0x10;  /* length */
   font[28]=font[29]=font[30]=font[31]=0;

   stbtt_fontinfo info;
   int rc = stbtt_InitFontEx(&info, font, 0, 32);
   printf("rc=%d\n", rc);
   free(font);
   return 0;
}
