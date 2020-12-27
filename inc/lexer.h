#ifndef __LEXER_H
#define __LEXER_H

#include <string>

enum class Token : int {
    TOK_EOF = -1,
    TOK_DEF = -2,
    TOK_EXTERN = -3,
    TOK_IDENTIFIER = -4,
    TOK_NUMBER = -5
};

// Filled in if TOK_IDENTIFIER.
extern std::string identifier_str;
// Filled in if TOK_NUMBER.
extern double num_val;

// Return the next token from standard input.
int gettok();

#endif // __LEXER_H