#include <cstdint>

#include "tree.h"

using namespace llvm;

std::unique_ptr<LLVMContext> the_context;
std::unique_ptr<Module> the_module;
std::unique_ptr<IRBuilder<>> builder;
std::map<std::string, Value*> named_values;

static Value* log_error_v(const char* str) {
    fprintf(stderr, "log_error: %s\n", str);
    return nullptr;
}

Value* Number_expr_AST::codegen() {
    return ConstantFP::get(*the_context, APFloat(val));
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
        return builder->CreateFAdd(l, r, "addtmp");
    case '-':
        return builder->CreateFSub(l, r, "subtmp");
    case '*':
        return builder->CreateFMul(l, r, "multmp");
    case '<':
        l = builder->CreateFCmpULT(l, r, "cmptmp");
        // Convert bool 0/1 to double 0.0 or 1.0.
        return builder->CreateUIToFP(l, Type::getDoubleTy(*the_context),
                                     "booltmp");
    default:
        return log_error_v("Invalid binary operator.");
    }
}

Value* Call_expr_AST::codegen() {
    // Look up the name in the global module table.
    Function* callee_f = the_module->getFunction(callee);
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

    return builder->CreateCall(callee_f, args_v, "calltmp");
}

Function* Prototype_AST::codegen() {
    // Make the function type: double(double, double) etc.
    std::vector<Type*> doubles(args.size(), Type::getDoubleTy(*the_context));
    FunctionType* ft =
            FunctionType::get(Type::getDoubleTy(*the_context), doubles, false);
    Function* f =
            Function::Create(ft, Function::ExternalLinkage, name, the_module.get());

    // Set the names for all arguments.
    uint32_t idx = 0;
    for (auto& arg : f->args())
        arg.setName(args[idx++]);

    return f;
}

Function* Function_AST::codegen() {
    // First, check for an existing function
    // from a previous 'extern' declaration.
    Function* the_function = the_module->getFunction(proto->get_name());

    if (!the_function)
        the_function = proto->codegen();

    if (!the_function)
        return nullptr;

    if (!the_function->empty())
        return (Function*)log_error_v("Function cannot be redefined.");

    // Create a new basic block to start insertion into.
    BasicBlock* bb = BasicBlock::Create(*the_context, "entry", the_function);
    builder->SetInsertPoint(bb);

    // Record the function arguments in the named_values map.
    named_values.clear();
    for (auto& arg : the_function->args())
        named_values[arg.getName()] = &arg;

    if (Value* ret_val = body->codegen()) {
        // Finish off the function.
        builder->CreateRet(ret_val);

        // Validate the generated code, checking for consistency.
        verifyFunction(*the_function);

        return the_function;
    }

    // Error reading the body, remove the function.
    the_function->eraseFromParent();
    return nullptr;
}
