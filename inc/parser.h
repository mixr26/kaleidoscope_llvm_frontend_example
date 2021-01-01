#ifndef __PARSER_H
#define __PARSER_H

#include <memory>

#include "tree.h"

// cur_tok/get_next_token - Provide a simple token buffer. cur_tok is the
// current token the parser is looking a. get_next_token reads another token
// from the lexer and updates cur_tok with its results.
extern int cur_tok;

int get_next_token();

std::unique_ptr<Function_AST> parse_definition();
std::unique_ptr<Prototype_AST> parse_extern();
std::unique_ptr<Function_AST> parse_top_level_expr();

#endif // __PARSER_H
