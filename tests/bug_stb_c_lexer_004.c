#define STB_C_LEXER_IMPLEMENTATION
#include "stb_c_lexer.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main()
{
    // Input: unterminated block comment ending with '*' at the last byte of input.
    // The comment end check reads p[1] at eof when p == eof-1 && p[0] == '*'.
    char *input = malloc(8);
    if (!input) return 1;
    memcpy(input, "/* abc *", 8);  // unterminated, last char is '*'

    char store[256];
    stb_lexer lex;

    stb_c_lexer_init(&lex, input, input + 8, store, sizeof(store));

    printf("Calling stb_c_lexer_get_token with unterminated block comment ending in '*'...\n");
    int result = stb_c_lexer_get_token(&lex);
    printf("Token: %ld, result: %d\n", lex.token, result);

    free(input);
    // Expected: parse error because comment is unterminated, or some other token
    // after skipping past the unterminated comment. Either way, no OOB read.
    return 0;
}
