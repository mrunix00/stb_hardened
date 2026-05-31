#define STB_C_LEXER_IMPLEMENTATION
#include "stb_c_lexer.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main()
{
    // Input: a lone single-quote character with no following character before eof.
    // The char literal case reads past eof in stb__clex_parse_char(p+1, &p).
    char *input = malloc(1);
    if (!input) return 1;
    input[0] = '\'';

    char store[256];
    stb_lexer lex;

    stb_c_lexer_init(&lex, input, input + 1, store, sizeof(store));

    printf("Calling stb_c_lexer_get_token with lone quote...\n");
    int result = stb_c_lexer_get_token(&lex);
    printf("Token: %ld, result: %d\n", lex.token, result);

    free(input);
    if (result == 1 && lex.token == CLEX_parse_error) {
        printf("Got parse error as expected\n");
        return 0;
    }

    fprintf(stderr, "BUG: lone quote did not produce parse error (token=%ld)\n", lex.token);
    return 1;
}
