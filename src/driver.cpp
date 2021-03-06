#include <cassert>

#include "driver.h"
#include "lexer.h"
#include "parser.h"

#include "llvm/Support/Error.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Utils.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"

void initialize_module() {
    // Open a new module.
    the_module = std::make_unique<Module>("my jit", the_context);
    the_module->setDataLayout(the_jit->getTargetMachine().createDataLayout());

    // Create a new pass manager attached to the module.
    the_fpm = std::make_unique<legacy::FunctionPassManager>(the_module.get());

    // Promote allocas to registers.
    the_fpm->add(createPromoteMemoryToRegisterPass());
    // Do simple "peephole" optimizations and bit-twiddling optzns.
    the_fpm->add(createInstructionCombiningPass());
    // Reassociate expressions.
    the_fpm->add(createReassociatePass());
    // CSE.
    the_fpm->add(createGVNPass());
    // Simplify the control flow graph (deleting unreachable blocks, etc).
    the_fpm->add(createCFGSimplificationPass());

    the_fpm->doInitialization();
}

static void handle_definition() {
    if (auto fn_ast = parse_definition()) {
        if (auto* fn_ir = fn_ast->codegen()) {
            fprintf(stderr, "Read function definition: ");
            fn_ir->print(errs());
            fprintf(stderr, "\n");
            the_jit->addModule(std::move(the_module));
            initialize_module();
        }
    } else
        // Skip token for error recovery.
        get_next_token();
}

static void handle_extern() {
    if (auto proto_ast = parse_extern()) {
        if (auto* fn_ir = proto_ast->codegen()) {
            fprintf(stderr, "Read extern: ");
            fn_ir->print(errs());
            fprintf(stderr, "\n");
            function_protos[proto_ast->get_name()] = std::move(proto_ast);
        }
    } else
        // Skip token for error recovery.
        get_next_token();
}

static void handle_top_level_expression() {
    // Evaluate a top-level expression into an anonymous function.
    if (auto fn_ast = parse_top_level_expr()) {
        if (fn_ast->codegen()) {
            // JIT the module containing the anonymous expression, keeping a
            // handle so we can free it later.
            auto h = the_jit->addModule(std::move(the_module));
            initialize_module();

            // Search the JIT for the __anon expr symbol.
            auto expr_symbol = the_jit->findSymbol("__anon_expr");
            assert(expr_symbol && "Function not found.");

            // Get the symbol's address and cast it to the right type (takes no
            // arguments, returns a double) so we can call it as a native
            // function.
            double (*fp)() = (double (*)())(intptr_t)cantFail(expr_symbol.getAddress());
            fprintf(stderr, "Evaluated to %f\n", fp());

            // Delete the anonymous expression module from the JIT.
            the_jit->removeModule(h);
        }
    } else
        // Skip token for error recovery.
        get_next_token();
}

// top ::= definition | external | expression | ';'
void main_loop() {
    while (true) {
        fprintf(stderr, "ready> ");
        switch (cur_tok) {
        case static_cast<int>(Token::TOK_EOF):
            return;
        case ';': // Ignore top-level semicolons.
            get_next_token();
            break;
        case static_cast<int>(Token::TOK_DEF):
            handle_definition();
            break;
        case static_cast<int>(Token::TOK_EXTERN):
            handle_extern();
            break;
        default:
            handle_top_level_expression();
            break;
        }
    }
}

void emit_object_code() {
    // Initialize the target registry etc.
    InitializeAllTargetInfos();
    InitializeAllTargets();
    InitializeAllTargetMCs();
    InitializeAllAsmParsers();
    InitializeAllAsmPrinters();

    auto target_triple = sys::getDefaultTargetTriple();
    the_module->setTargetTriple(target_triple);

    std::string error;
    auto target = TargetRegistry::lookupTarget(target_triple, error);

    if (!target) {
        errs() << error;
        return;
    }

    auto cpu = "generic";
    auto features = "";

    TargetOptions opt;
    auto rm = Optional<Reloc::Model>();
    auto the_target_machine =
            target->createTargetMachine(target_triple, cpu, features, opt, rm);

    the_module->setDataLayout(the_target_machine->createDataLayout());

    auto filename = "output.o";
    std::error_code ec;
    raw_fd_ostream dest(filename, ec, sys::fs::OF_None);

    if (ec) {
        errs() << "Could not open file: " << ec.message();
        return;
    }

    legacy::PassManager pass;
    auto file_type = LLVMTargetMachine::CGFT_ObjectFile;

    if (the_target_machine->addPassesToEmitFile(pass, dest, nullptr, file_type)) {
        errs() << "Can't emit this type of file.";
        return;
    }

    pass.run(*the_module);
    dest.flush();

    outs() << "Wrote " << filename << "\n";

    return;
}
