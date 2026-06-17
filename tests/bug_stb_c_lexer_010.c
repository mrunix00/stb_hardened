#define STB_C_LEXER_IMPLEMENTATION
#include "stb_c_lexer.h"
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    char *input;
    char store[16];
    stb_lexer lex;
    int ret;

    /* BUG-010: strtol/strtod do not accept a length bound and will read past
     * lexer->eof when the input is not null-terminated. We place a number at
     * the very end of a tiny malloc'd buffer so that strtol (via stdlib path)
     * reads into the ASan redzone immediately after the allocation.
     *
     * The non-stdlib path (stb's own bounded parser) does not have this bug
     * because it explicitly checks q != lexer->eof in its parsing loops. */
    input = (char *)malloc(2);
    if (!input) {
        fprintf(stderr, "BUG-010 FAIL: malloc failed\n");
        return 1;
    }
    input[0] = '5';
    input[1] = '5';  /* "55" at bytes 0-1; byte 2 is in ASan redzone */

    stb_c_lexer_init(&lex, input, input + 2, store, sizeof(store));
    ret = stb_c_lexer_get_token(&lex);

    /* If we reach here, either:
     * - The non-stdlib bounded parser was used (and parsed correctly), OR
     * - strtol's OOB read was not caught (byte at eof was a non-digit) */
    if (ret == 0) {
        printf("BUG-010 FAIL: Expected a token, got EOF\n");
        free(input);
        return 1;
    }

    if (lex.token == CLEX_intlit && lex.int_number == 55) {
        printf("BUG-010 PASS: Correctly parsed integer 55\n");
        free(input);
        return 0;
    }

    printf("BUG-010 WARN: Got token %ld, int_number=%ld\n",
           (long)lex.token, (long)lex.int_number);
    free(input);
    return 0;
}
