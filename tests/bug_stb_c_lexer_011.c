// Regression test for BUG-stb_c_lexer-011
// OOB read in stb__clex_parse_suffixes when a numeric token ends exactly
// at lexer->eof and suffix parsing is enabled.
#define STB_C_LEX_PARSE_SUFFIXES    Y
#define STB_C_LEX_C_DECIMAL_INTS    Y
#define STB_C_LEX_C_HEX_INTS        Y
#define STB_C_LEX_C_OCTAL_INTS      Y
#define STB_C_LEX_C_DECIMAL_FLOATS  Y
#define STB_C_LEX_C99_HEX_FLOATS    N
#define STB_C_LEX_C_IDENTIFIERS     Y
#define STB_C_LEX_C_DQ_STRINGS      Y
#define STB_C_LEX_C_SQ_STRINGS      N
#define STB_C_LEX_C_CHARS           Y
#define STB_C_LEX_C_COMMENTS        Y
#define STB_C_LEX_CPP_COMMENTS      Y
#define STB_C_LEX_C_COMPARISONS     Y
#define STB_C_LEX_C_LOGICAL         Y
#define STB_C_LEX_C_SHIFTS          Y
#define STB_C_LEX_C_INCREMENTS      Y
#define STB_C_LEX_C_ARROW           Y
#define STB_C_LEX_EQUAL_ARROW       N
#define STB_C_LEX_C_BITWISEEQ       Y
#define STB_C_LEX_C_ARITHEQ         Y
#define STB_C_LEX_DECIMAL_SUFFIXES  "uUlL"
#define STB_C_LEX_HEX_SUFFIXES      "lL"
#define STB_C_LEX_OCTAL_SUFFIXES    "lL"
#define STB_C_LEX_FLOAT_SUFFIXES    "fF"
#define STB_C_LEX_0_IS_EOF          N
#define STB_C_LEX_INTEGERS_AS_DOUBLES  N
#define STB_C_LEX_MULTILINE_DSTRINGS   N
#define STB_C_LEX_MULTILINE_SSTRINGS   N
#define STB_C_LEX_USE_STDLIB           N
#define STB_C_LEX_DOLLAR_IDENTIFIER    Y
#define STB_C_LEX_FLOAT_NO_DECIMAL     Y
#define STB_C_LEX_DEFINE_ALL_TOKEN_NAMES  Y
#define STB_C_LEX_DISCARD_PREPROCESSOR    Y
#define STB_C_LEXER_DEFINITIONS
#define STB_C_LEXER_IMPLEMENTATION
#include "stb_c_lexer.h"
#include <stdlib.h>

int main(void)
{
    char *input = (char *)malloc(1);
    if (!input) return 1;
    input[0] = '1';

    char *store = (char *)malloc(256);
    if (!store) {
        free(input);
        return 1;
    }

    stb_lexer lex;
    stb_c_lexer_init(&lex, input, input + 1, store, 256);
    while (stb_c_lexer_get_token(&lex)) {
        if (lex.token == CLEX_parse_error)
            break;
    }

    free(input);
    free(store);
    return 0;
}
