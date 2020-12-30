#include "lexer.h"
#include "parser.h"

#include <iostream>

int main() {
    // Prime the first token.
    fprintf(stderr, "ready> ");
    get_next_token();

    // Run the interpreter loop.
    main_loop();

    return 0;
}
