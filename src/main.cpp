#include "driver.h"
#include "lexer.h"
#include "parser.h"

#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"

int main() {
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();

    // Prime the first token.
    fprintf(stderr, "ready> ");
    get_next_token();

    the_jit = std::make_unique<KaleidoscopeJIT>();
    initialize_module();

    // Run the interpreter loop.
    main_loop();

    // Emit the object code
    emit_object_code();

    return 0;
}
