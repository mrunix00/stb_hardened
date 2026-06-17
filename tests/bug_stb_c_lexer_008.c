#define STB_C_LEXER_IMPLEMENTATION
#include "stb_c_lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    char *input;
    char store[64];
    stb_lexer lex;
    int ret;

    /* BUG-008: When the character before lexer->eof is a backslash during
     * string parsing, stb__clex_parse_char(p, &q) is called where p is
     * at eof-1. The function reads p[1] at eof (one past buffer) to
     * determine the escape type in its switch(p[1]) statement.
     *
     * We use a malloc'd 2-byte buffer so ASan detects the OOB read
     * at offset 2 (the redzone). */
    input = (char *)malloc(2);
    if (!input) {
        fprintf(stderr, "BUG-008 FAIL: malloc failed\n");
        return 1;
    }
    input[0] = '"';   /* opening quote */
    input[1] = '\\';  /* backslash at last byte before eof — triggers OOB read */

    stb_c_lexer_init(&lex, input, input + 2, store, sizeof(store));
    ret = stb_c_lexer_get_token(&lex);

    /* If we reach here, the OOB read was not fatal (e.g. no ASan, or byte
     * at eof happened to not cause a crash). The expected behaviour with
     * a correct fix is that the tokenizer sees the unterminated string
     * with a trailing backslash and returns CLEX_parse_error. */
    if (ret == 0) {
        printf("BUG-008 FAIL: Expected a token (parse error or string), got EOF\n");
        free(input);
        return 1;
    }

    if (lex.token == CLEX_parse_error) {
        printf("BUG-008 PASS: Correctly returned CLEX_parse_error for unterminated escape\n");
        free(input);
        return 0;
    }

    printf("BUG-008 FAIL: Got token %ld instead of CLEX_parse_error\n", lex.token);
    free(input);
    return 1;
}
