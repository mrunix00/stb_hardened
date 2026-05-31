#define STB_C_LEXER_IMPLEMENTATION
#include "stb_c_lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main()
{
    // Input with an unterminated double-quoted string — the closing " is missing.
    // With ASan, the OOB read past input[] will be detected.
    char input[] = "\"hello world this string has no terminator";
    char store[256];
    stb_lexer lex;

    stb_c_lexer_init(&lex, input, input + sizeof(input) - 1, store, sizeof(store));

    printf("Calling stb_c_lexer_get_token with unterminated string...\n");
    int result = stb_c_lexer_get_token(&lex);
    printf("Token: %ld, result: %d\n", lex.token, result);

    // If we get here without crashing, check that the token is a parse error
    // (the string_storage overflow check at line 487 should catch it before
    // the loop exits, but the OOB read still occurs)
    if (result == 1 && lex.token == CLEX_parse_error) {
        printf("Got parse error as expected (after OOB read)\n");
        return 0;
    }

    // If we somehow parsed something unexpected, the bug may still be latent
    fprintf(stderr, "BUG: unterminated string did not produce parse error (token=%ld)\n", lex.token);
    return 1;
}
