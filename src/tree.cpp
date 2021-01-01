#include <cstdint>

#include "tree.h"

using namespace llvm;
using namespace llvm::orc;

LLVMContext the_context;
std::unique_ptr<Module> the_module;
IRBuilder<> builder(the_context);
std::map<std::string, Value*> named_values;
std::unique_ptr<legacy::FunctionPassManager> the_fpm;
std::unique_ptr<KaleidoscopeJIT> the_jit;
std::map<std::string, std::unique_ptr<Prototype_AST>> function_protos;

static Value* log_error_v(const char* str) {
    fprintf(stderr, "log_error: %s\n", str);
    return nullptr;
}

Value* Number_expr_AST::codegen() {
    return ConstantFP::get(the_context, APFloat(val));
}

Value* Variable_expr_AST::codegen() {
    // Look this variable up in the function.
    Value* v = named_values[name];
    if (!v)
        log_error_v("Unknown variable name.");
    return v;
}

Value* Binary_expr_AST::codegen() {
    Value* l = lhs->codegen();
    Value* r = rhs->codegen();

    if (!l || !r)
        return nullptr;

    switch (op) {
    case '+':
        return builder.CreateFAdd(l, r, "addtmp");
    case '-':
        return builder.CreateFSub(l, r, "subtmp");
    case '*':
        return builder.CreateFMul(l, r, "multmp");
    case '<':
        l = builder.CreateFCmpULT(l, r, "cmptmp");
        // Convert bool 0/1 to double 0.0 or 1.0.
        return builder.CreateUIToFP(l, Type::getDoubleTy(the_context),
                                    "booltmp");
    default:
        return log_error_v("Invalid binary operator.");
    }
}

Function* get_function(std::string name) {
    // First, see if the function has already been added to the current module.
    if (auto* f = the_module->getFunction(name))
        return f;

    // If not, check whether we can codegen the declaration from some existing
    // prototype.
    auto fi = function_protos.find(name);
    if (fi != function_protos.end())
        return fi->second->codegen();

    // If no existing prototype exists, return null.
    return nullptr;
}

Value* Call_expr_AST::codegen() {
    // Look up the name in the global module table.
    Function* callee_f = get_function(callee);
    if (!callee_f)
        return log_error_v("Unknown function referenced.");

    // If argument mismatch error.
    if (callee_f->arg_size() != args.size())
        return log_error_v("Incorrect number of arguments passed.");

    std::vector<Value*> args_v;
    for (uint32_t i = 0, e = args.size(); i != e; ++i) {
        args_v.emplace_back(args[i]->codegen());
        if (!args_v.back())
            return nullptr;
    }

    return builder.CreateCall(callee_f, args_v, "calltmp");
}

Function* Prototype_AST::codegen() {
    // Make the function type: double(double, double) etc.
    std::vector<Type*> doubles(args.size(), Type::getDoubleTy(the_context));
    FunctionType* ft =
            FunctionType::get(Type::getDoubleTy(the_context), doubles, false);
    Function* f =
            Function::Create(ft, Function::ExternalLinkage, name, the_module.get());

    // Set the names for all arguments.
    uint32_t idx = 0;
    for (auto& arg : f->args())
        arg.setName(args[idx++]);

    return f;
}

Function* Function_AST::codegen() {
    // Transfer ownership of the prototype to te function_protos map, but keep a
    // reference to it for use below.
    auto& p = *proto;
    function_protos[proto->get_name()] = std::move(proto);
    Function* the_function = get_function(p.get_name());

    if (!the_function)
        return nullptr;

    // Create a new basic block to start insertion into.
    BasicBlock* bb = BasicBlock::Create(the_context, "entry", the_function);
    builder.SetInsertPoint(bb);

    // Record the function arguments in the named_values map.
    named_values.clear();
    for (auto& arg : the_function->args())
        named_values[arg.getName()] = &arg;

    if (Value* ret_val = body->codegen()) {
        // Finish off the function.
        builder.CreateRet(ret_val);

        // Validate the generated code, checking for consistency.
        verifyFunction(*the_function);

        // Optimize the function.
        the_fpm->run(*the_function);

        return the_function;
    }

    // Error reading the body, remove the function.
    the_function->eraseFromParent();
    return nullptr;
}

//===----------------------------------------------------------------------===//
// "Library" functions that can be "extern'd" from user code.
//===----------------------------------------------------------------------===//

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

/// putchard - putchar that takes a double and returns 0.
extern "C" DLLEXPORT double putchard(double X) {
  fputc((char)X, stderr);
  return 0;
}

/// printd - printf that takes a double prints it as "%f\n", returning 0.
extern "C" DLLEXPORT double printd(double X) {
  fprintf(stderr, "%f\n", X);
  return 0;
}

