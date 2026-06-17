// Regression test for BUG-stb_c_lexer-012
// OOB read in stb__clex_parse_float when a decimal point is the last
// character of the input buffer (e.g. "8." at end of file).
//
// The lexer's float-parsing function unconditionally enters a for loop
// after matching '.' and reads *p without checking p != eof. When '.'
// is the last byte, p advances past eof and the loop dereferences one
// byte past the buffer.
#define STB_C_LEXER_IMPLEMENTATION
#include "stb_c_lexer.h"
#include <stdlib.h>
#include <string.h>

int main(void)
{
    // Allocate exactly 2 bytes — no trailing null, eof points just past '.'
    char *input = (char *)malloc(2);
    if (!input) return 1;
    input[0] = '8';
    input[1] = '.';

    char *store = (char *)malloc(256);
    if (!store) { free(input); return 1; }

    stb_lexer lex;
    stb_c_lexer_init(&lex, input, input + 2, store, 256);
    while (stb_c_lexer_get_token(&lex)) {
        if (lex.token == CLEX_parse_error)
            break;
    }

    free(input);
    free(store);
    return 0;
}
