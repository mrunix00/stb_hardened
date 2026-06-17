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

    /* BUG-009: In the char-literal parsing branch, stb__clex_parse_char(p+1, &p)
     * is called where p+1 == lexer->eof-1. If *(eof-1) == '\\', the function
     * reads p[1] at eof (one byte past buffer) in its switch(p[1]) statement.
     *
     * We use a malloc'd 2-byte buffer so ASan detects the OOB read. */
    input = (char *)malloc(2);
    if (!input) {
        fprintf(stderr, "BUG-009 FAIL: malloc failed\n");
        return 1;
    }
    input[0] = '\'';  /* opening single quote */
    input[1] = '\\';  /* backslash at eof-1 — triggers OOB read in parse_char */

    stb_c_lexer_init(&lex, input, input + 2, store, sizeof(store));
    ret = stb_c_lexer_get_token(&lex);

    /* If we reach here, the OOB read was not fatal.
     * Expected correct behaviour: CLEX_parse_error for malformed char literal. */
    if (ret == 0) {
        printf("BUG-009 FAIL: Expected a token, got EOF\n");
        free(input);
        return 1;
    }

    if (lex.token == CLEX_parse_error) {
        printf("BUG-009 PASS: Correctly returned CLEX_parse_error for malformed char literal\n");
        free(input);
        return 0;
    }

    printf("BUG-009 FAIL: Got token %ld instead of CLEX_parse_error\n", lex.token);
    free(input);
    return 1;
}
