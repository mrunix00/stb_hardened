#define STB_C_LEXER_IMPLEMENTATION
#include "stb_c_lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    char input[3];
    input[0] = '\'';
    input[1] = 'a';
    input[2] = '\'';

    char store[256];
    stb_lexer lex;
    stb_c_lexer_init(&lex, input, input + 3, store, sizeof(store));

    int result = stb_c_lexer_get_token(&lex);
    if (!result) {
        printf("FAIL: expected charlit token, got EOF\n");
        return 1;
    }
    if (lex.token != CLEX_charlit) {
        printf("FAIL: expected CLEX_charlit (%d), got %ld\n", CLEX_charlit, lex.token);
        return 1;
    }

    result = stb_c_lexer_get_token(&lex);
    if (result != 0) {
        printf("FAIL: expected EOF (0), got token %ld\n", lex.token);
        return 1;
    }

    printf("PASS: BUG-006 not reproducible (no crash/OOB)\n");
    return 0;
}
