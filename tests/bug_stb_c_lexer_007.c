#define STB_C_LEXER_IMPLEMENTATION
#include "stb_c_lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    char input[] = "'A';";
    char store[256];
    stb_lexer lex;
    stb_c_lexer_init(&lex, input, input + strlen(input), store, sizeof(store));

    int r1 = stb_c_lexer_get_token(&lex);
    if (!r1 || lex.token != CLEX_charlit) {
        printf("FAIL: expected CLEX_charlit, got token=%ld\n", lex.token);
        return 1;
    }
    printf("Token 1: CLEX_charlit (where_firstchar=%td, where_lastchar=%td, len=%td)\n",
           lex.where_firstchar - (char*)input,
           lex.where_lastchar - (char*)input,
           lex.where_lastchar - lex.where_firstchar);

    int r2 = stb_c_lexer_get_token(&lex);
    if (!r2) {
        printf("FAIL: expected ';' token, got EOF (char literal consumed extra char)\n");
        return 1;
    }
    if (lex.token != ';') {
        printf("FAIL: expected ';' (59), got token %ld\n", lex.token);
        return 1;
    }
    printf("Token 2: ';'\n");

    printf("PASS: BUG-007 not reproducible (char literal span is correct)\n");
    return 0;
}
