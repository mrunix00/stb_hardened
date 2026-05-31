#define STB_C_LEXER_IMPLEMENTATION
#include "stb_c_lexer.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main()
{
    // Allocate a small buffer whose content is entirely identifier characters.
    // eof points to the end of allocated memory. The identifier loop reads
    // past eof into unmapped or redzoned memory.
    char *input = malloc(8);
    if (!input) return 1;
    memcpy(input, "abcd1234", 8);  // all identifier chars

    char store[256];
    stb_lexer lex;

    stb_c_lexer_init(&lex, input, input + 8, store, sizeof(store));

    printf("Calling stb_c_lexer_get_token with identifier filling whole buffer...\n");
    int result = stb_c_lexer_get_token(&lex);
    printf("Token: %ld, result: %d\n", lex.token, result);

    // If we get here without ASan catching it, check that we at least
    // returned some reasonable token and the identifier was consumed.
    free(input);
    if (result == 1 && lex.token == CLEX_id) {
        printf("Got identifier '%s' (len=%d)\n", lex.string, lex.string_len);
        return 0;
    }
    if (result == 1 && lex.token == CLEX_parse_error) {
        printf("Got parse error (string_storage full or OOB caught by bounds)\n");
        return 0;
    }

    fprintf(stderr, "BUG: unexpected token result %d (token=%ld)\n", result, lex.token);
    return 1;
}
