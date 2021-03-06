#include <cstdint>

#include "tree.h"
#include "parser.h"

using namespace llvm;
using namespace llvm::orc;

LLVMContext the_context;
std::unique_ptr<Module> the_module;
IRBuilder<> builder(the_context);
std::map<std::string, AllocaInst*> named_values;
std::unique_ptr<legacy::FunctionPassManager> the_fpm;
std::unique_ptr<KaleidoscopeJIT> the_jit;
std::map<std::string, std::unique_ptr<Prototype_AST>> function_protos;

Function* get_function(std::string name);

static Value* log_error_v(const char* str) {
    fprintf(stderr, "log_error: %s\n", str);
    return nullptr;
}

static AllocaInst* create_entry_block_alloca(Function* the_function,
                                             const std::string& var_name) {
    IRBuilder<> tmp_b(&the_function->getEntryBlock(),
                      the_function->getEntryBlock().begin());
    return tmp_b.CreateAlloca(Type::getDoubleTy(the_context), 0,
                              var_name.c_str());
}

Value* Number_expr_AST::codegen() {
    return ConstantFP::get(the_context, APFloat(val));
}

Value* Variable_expr_AST::codegen() {
    // Look this variable up in the function.
    Value* v = named_values[name];
    if (!v)
        log_error_v("Unknown variable name.");

    // Load the value.
    return builder.CreateLoad(v, name.c_str());
}

Value* Unary_expr_AST::codegen() {
    Value* operand_v = operand->codegen();
    if (!operand_v)
        return nullptr;

    Function* f = get_function(std::string("unary") + opcode);
    if (!f)
        return log_error_v("Unknown unary operator.");

    return builder.CreateCall(f, operand_v, "unop");
}

Value* Binary_expr_AST::codegen() {
    // Special case '=', because we don't want to emit the LHS as an expression.
    if (op == '=') {
        // Assignment requires the LHS to be an identifier.
        Variable_expr_AST* lhse = (Variable_expr_AST*)lhs.get();
        if (!lhse)
            return log_error_v("Destination of '=' must be a variable.");

        // Codegen the RHS.
        Value* val = rhs->codegen();
        if (!val)
            return nullptr;

        // Look up the name.
        Value* variable = named_values[lhse->get_name()];
        if (!variable)
            return log_error_v("Unknown variable name.");

        builder.CreateStore(val, variable);
        return val;
    }

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
        break;
    }

    // If it wasn't a builtin binary operator, it must be a user defined one.
    // Emit a call to it.
    Function* f = get_function(std::string("binary") + op);
    assert(f && "binary operator not found!");

    Value* ops[2] = { l, r };
    return builder.CreateCall(f, ops, "binop");
}

Value* If_expr_AST::codegen() {
    Value* cond_v = cond->codegen();
    if (!cond_v)
        return nullptr;

    // Convert condition to a bool by comparing non-equal to 0.0.
    cond_v = builder.CreateFCmpONE(cond_v,
                                   ConstantFP::get(the_context, APFloat(0.0)),
                                   "ifcond");

    Function* the_function = builder.GetInsertBlock()->getParent();

    // Create blocks for the then and else cases. Insert the 'then' block at
    // the end of the function.
    BasicBlock* then_bb = BasicBlock::Create(the_context, "then", the_function);
    BasicBlock* else_bb = BasicBlock::Create(the_context, "else");
    BasicBlock* merge_bb = BasicBlock::Create(the_context, "ifcont");

    builder.CreateCondBr(cond_v, then_bb, else_bb);

    // Emit then value.
    builder.SetInsertPoint(then_bb);

    Value* then_v = then->codegen();
    if (!then_v)
        return nullptr;

    builder.CreateBr(merge_bb);
    // Codegen of 'then' can change the current block, update then_bb
    // for the PHI.
    then_bb = builder.GetInsertBlock();

    // Emit else block.
    the_function->getBasicBlockList().push_back(else_bb);
    builder.SetInsertPoint(else_bb);

    Value* else_v = elze->codegen();
    if (!else_v)
        return nullptr;

    builder.CreateBr(merge_bb);
    // Codege of 'else' can change the current block, update else_bb
    // for the PHI.
    else_bb = builder.GetInsertBlock();

    // Emit merge block.
    the_function->getBasicBlockList().push_back(merge_bb);
    builder.SetInsertPoint(merge_bb);
    PHINode* pn = builder.CreatePHI(Type::getDoubleTy(the_context), 2, "iftmp");
    pn->addIncoming(then_v, then_bb);
    pn->addIncoming(else_v, else_bb);

    return pn;
}

// Output for-loop as:
//  var = alloca double
//  ...
//  start = startexpr
//  store start -> var
//  goto loop
// loop:
//  ...
//  bodyexpr
//  ...
// loopend:
//  step = stepexpr
//  endcond = endexpr
//
//  curvar = load var
//  nextvar = curvar + step
//  store nextvar -> va
//  br endcond, loop, endloop
// endloop:
//  ...
Value* For_expr_AST::codegen() {
    Function* the_function = builder.GetInsertBlock()->getParent();

    // Create an alloca for the variable in the entry block.
    AllocaInst* alloca_var = create_entry_block_alloca(the_function, var_name);

    // Emit the start code first, without 'variable' in scope.
    Value* start_val = start->codegen();
    if (!start_val)
        return nullptr;

    // Store the value into the alloca.
    builder.CreateStore(start_val, alloca_var);

    // Make the new basic block for the loop header, inserting after current
    // block.
    BasicBlock* loop_bb = BasicBlock::Create(the_context, "loop", the_function);

    // Insert an explicit fall through from the current block to the loop_bb.
    builder.CreateBr(loop_bb);

    // Start insertion in loop_bb;
    builder.SetInsertPoint(loop_bb);

    // Within the loop, the variable is defined equal to the PHI node. If it
    // shadows an existing variable, we have to restore it, so save it now.
    AllocaInst* old_val = named_values[var_name];
    named_values[var_name] = alloca_var;

    // Emit the body of the loop. This, like any other expr, can change the
    // current BB. Note that we ignore the value computed by the body, but
    // don't allow an error.
    if (!body->codegen())
        return nullptr;

    // Emit the step value.
    Value* step_val = nullptr;
    if (step) {
        step_val = step->codegen();
        if (!step_val)
            return nullptr;
    } else
        // If not specified, use 1.0.
        step_val = ConstantFP::get(the_context, APFloat(1.0));

    // Compute the end condition.
    Value* end_cond = end->codegen();
    if (!end_cond)
        return nullptr;

    // Reload, increment and restore the alloca. This handles the case where the
    // body of the loop mutates the variable.
    Value* cur_var = builder.CreateLoad(alloca_var, var_name.c_str());
    Value* next_var = builder.CreateFAdd(cur_var, step_val, "nextvar");
    builder.CreateStore(next_var, alloca_var);

    // Convert condition to a bool by comparing non-equal to 0.0.
    end_cond = builder.CreateFCmpONE(end_cond,
                                     ConstantFP::get(the_context, APFloat(0.0)),
                                     "loopcond");

    // Create the "after loop" block and insert it.
    BasicBlock* after_bb =
            BasicBlock::Create(the_context, "afterloop", the_function);

    // Insert the conditional branch into the end of loop_end_bb;
    builder.CreateCondBr(end_cond, loop_bb, after_bb);

    // Any new code will be inserted inf after_bb.
    builder.SetInsertPoint(after_bb);

    // Restore the unshadowed variable.
    if (old_val)
        named_values[var_name] = old_val;
    else
        named_values.erase(var_name);

    // For expr always returns 0.0.
    return Constant::getNullValue(Type::getDoubleTy(the_context));
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

    // If this is an operator, install it.
    if (p.is_binary_op())
        binop_precedence[p.get_operator_name()] = p.get_binary_precedence();

    // Create a new basic block to start insertion into.
    BasicBlock* bb = BasicBlock::Create(the_context, "entry", the_function);
    builder.SetInsertPoint(bb);

    // Record the function arguments in the named_values map.
    named_values.clear();
    for (auto& arg : the_function->args()) {
        // Create an alloca for this variable.
        AllocaInst* alloca_var = create_entry_block_alloca(the_function,
                                                           arg.getName());

        // Store the initial value into the alloca.
        builder.CreateStore(&arg, alloca_var);

        // Add argument to the variable symbol table.
        named_values[arg.getName()] = alloca_var;
    }

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

Value* Var_expr_AST::codegen() {
    std::vector<AllocaInst*> old_bindings;

    Function* the_function = builder.GetInsertBlock()->getParent();

    // Register all variables and emit their initializer.
    for (uint32_t i = 0, e = var_names.size(); i != e; i++) {
        const std::string& var_name = var_names[i].first;
        Expr_AST* init = var_names[i].second.get();

        // Emit the initializer before adding the variable to scope, this
        // prevents the initializer from referencing the variable itself.
        Value* init_val;
        if (init) {
            init_val = init->codegen();
            if (!init_val)
                return nullptr;
        } else
            // If not specifier, use 0.0.
            init_val = ConstantFP::get(the_context, APFloat(0.0));

        AllocaInst* alloca_var = create_entry_block_alloca(the_function,
                                                           var_name);
        builder.CreateStore(init_val, alloca_var);

        // Remember the old variable binding so that we can restore the
        // binding when we unrecurse.
        old_bindings.push_back(named_values[var_name]);

        // Remember this binding.
        named_values[var_name] = alloca_var;
    }

    // Codegen the body.
    Value* body_val = body->codegen();
    if (!body_val)
        return nullptr;

    // Pop all our variables from scope.
    for (uint32_t i = 0, e = var_names.size(); i != e; i++)
        named_values[var_names[i].first] = old_bindings[i];

    // Return the body computation.
    return body_val;
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

