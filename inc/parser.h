#ifndef __PARSER_H
#define __PARSER_H

#include <map>

#include "tree.h"

// top ::= definition | external | expression | ';'
void main_loop();

int get_next_token();

#endif // __PARSER_H
