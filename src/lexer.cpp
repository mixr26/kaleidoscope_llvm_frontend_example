#include <cstdio>
#include <cctype>
#include <cstdlib>

#include "lexer.h"

std::string identifier_str;
double num_val;

// Return the next token from standard input.
int gettok() {
    static int last_char = ' ';

    // Skip any whitespace.
    while (isspace(last_char))
        last_char = getchar();

    if (isalpha(last_char)) {
        identifier_str = last_char;
        while (isalnum((last_char = getchar())))
            identifier_str += last_char;

        if (identifier_str == "def")
            return static_cast<int>(Token::TOK_DEF);
        if (identifier_str == "extern")
            return static_cast<int>(Token::TOK_EXTERN);
        if (identifier_str == "if")
            return static_cast<int>(Token::TOK_IF);
        if (identifier_str == "else")
            return static_cast<int>(Token::TOK_ELSE);
        if (identifier_str == "then")
            return static_cast<int>(Token::TOK_THEN);
        if (identifier_str == "for")
            return static_cast<int>(Token::TOK_FOR);
        if (identifier_str == "in")
            return static_cast<int>(Token::TOK_IN);
        if (identifier_str == "binary")
            return static_cast<int>(Token::TOK_BINARY);
        if (identifier_str == "unary")
            return static_cast<int>(Token::TOK_UNARY);
        if (identifier_str == "var")
            return static_cast<int>(Token::TOK_VAR);
        return static_cast<int>(Token::TOK_IDENTIFIER);
    }

    if (isdigit(last_char) || last_char == '.') {
        std::string num_str;
        do {
            num_str += last_char;
            last_char = getchar();
        } while (isdigit(last_char) || last_char == '.');

        num_val = strtod(num_str.c_str(), 0);
        return static_cast<int>(Token::TOK_NUMBER);
    }

    if (last_char == '#') {
        // Comment until EOL.
        do
            last_char = getchar();
        while (last_char != EOF && last_char != '\n' && last_char != '\r');

        if (last_char != EOF)
            return gettok();
    }

    // Check for EOF. Don't eat it.
    if (last_char == EOF)
        return static_cast<int>(Token::TOK_EOF);

    // Otherwise, just return the character as its ascii value.
    int this_char = last_char;
    last_char = getchar();
    return this_char;
}
