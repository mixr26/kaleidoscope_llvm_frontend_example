#include "lexer.h"
#include "parser.h"

#include <iostream>

static void initialize_module() {
    // Open a new context and module.
    the_context = std::make_unique<LLVMContext>();
    the_module = std::make_unique<Module>("my jit", *the_context);

    // Create a new builder for the module.
    builder = std::make_unique<IRBuilder<>>(*the_context);
}

int main() {
    // Prime the first token.
    fprintf(stderr, "ready> ");
    get_next_token();

    // Make the module, which holds all the code.
    initialize_module();

    // Run the interpreter loop.
    main_loop();

    // Print out all of the generated code.
    the_module->print(errs(), nullptr);

    return 0;
}
