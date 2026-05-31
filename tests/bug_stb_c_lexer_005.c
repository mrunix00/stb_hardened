#define STB_C_LEXER_IMPLEMENTATION
#include "stb_c_lexer.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main()
{
    // Input: a lone '/' at the end of the allocated buffer.
    // The CPP comment detection check reads p[1] at eof.
    char *input = malloc(1);
    if (!input) return 1;
    input[0] = '/';

    char store[256];
    stb_lexer lex;

    stb_c_lexer_init(&lex, input, input + 1, store, sizeof(store));

    printf("Calling stb_c_lexer_get_token with lone '/'...\n");
    int result = stb_c_lexer_get_token(&lex);
    printf("Token: %ld, result: %d\n", lex.token, result);

    free(input);
    // Expected: '/' returned as a single-char token, not an OOB read.
    return 0;
}
