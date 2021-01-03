#ifndef __TREE_H
#define __TREE_H

#include <memory>
#include <vector>

#include "KaleidoscopeJIT.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/Value.h"

using namespace llvm;
using namespace llvm::orc;

// Forward decl.
class Prototype_AST;

extern LLVMContext the_context;
extern std::unique_ptr<Module> the_module;
extern IRBuilder<> builder;
extern std::map<std::string, AllocaInst*> named_values;
extern std::unique_ptr<legacy::FunctionPassManager> the_fpm;
extern std::unique_ptr<KaleidoscopeJIT> the_jit;
extern std::map<std::string, std::unique_ptr<Prototype_AST>> function_protos;

// ExprAST - Base class for all expr nodes.
class Expr_AST {
public:
    virtual ~Expr_AST() {}
    virtual Value* codegen() = 0;
};

// Number_expr_AST - Expression class for numeric literals like "1.0".
class Number_expr_AST : public Expr_AST {
    double val;

public:
    Number_expr_AST(double val) : val(val) {}
    Value* codegen() override;
};

// Variable_expr_AST - Expression class for referencing a variable, like "a".
class Variable_expr_AST : public Expr_AST {
    std::string name;

public:
    Variable_expr_AST(const std::string& name) : name(name) {}
    const std::string& get_name() const { return name; }

    Value* codegen() override;
};

// Unary_expr_AST - Expression class for a unary operator.
class Unary_expr_AST : public Expr_AST {
    char opcode;
    std::unique_ptr<Expr_AST> operand;

public:
    Unary_expr_AST(char opcode, std::unique_ptr<Expr_AST> operand)
        : opcode(opcode), operand(std::move(operand)) {}

    Value* codegen() override;
};

// Binary_expr_AST - Expression class for a binary operator.
class Binary_expr_AST : public Expr_AST {
    char op;
    std::unique_ptr<Expr_AST> lhs;
    std::unique_ptr<Expr_AST> rhs;

public:
    Binary_expr_AST(char op, std::unique_ptr<Expr_AST> lhs,
                    std::unique_ptr<Expr_AST> rhs)
        : op(op), lhs(std::move(lhs)), rhs(std::move(rhs)) {}
    Value* codegen() override;
};

// Call_expr_AST - Expression class for function calls.
class Call_expr_AST : public Expr_AST {
    std::string callee;
    std::vector<std::unique_ptr<Expr_AST>> args;

public:
    Call_expr_AST(const std::string& callee,
                  std::vector<std::unique_ptr<Expr_AST>> args)
        : callee(callee), args(std::move(args)) {}
    Value* codegen() override;
};

// If_expr_AST - Expression class for if/then/else.
class If_expr_AST : public Expr_AST {
    std::unique_ptr<Expr_AST> cond;
    std::unique_ptr<Expr_AST> then;
    std::unique_ptr<Expr_AST> elze;

public:
    If_expr_AST(std::unique_ptr<Expr_AST> cond, std::unique_ptr<Expr_AST>(then),
                std::unique_ptr<Expr_AST> elze)
        : cond(std::move(cond)), then(std::move(then)), elze(std::move(elze)) {}

    Value* codegen() override;
};

// For_expr_AST - Expression class for for/in.
class For_expr_AST : public Expr_AST {
    std::string var_name;
    std::unique_ptr<Expr_AST> start;
    std::unique_ptr<Expr_AST> end;
    std::unique_ptr<Expr_AST> step;
    std::unique_ptr<Expr_AST> body;

public:
    For_expr_AST(const std::string& var_name, std::unique_ptr<Expr_AST> start,
                 std::unique_ptr<Expr_AST> end, std::unique_ptr<Expr_AST> step,
                 std::unique_ptr<Expr_AST> body)
        : var_name(var_name), start(std::move(start)), end(std::move(end)),
          step(std::move(step)), body(std::move(body)) {}

    Value* codegen() override;
};

// Prototype_AST - this class represents the "prototype" for a function,
// which captures its name, and its argument names (thus implicitly the
// number of arguments the function takes).
class Prototype_AST {
    std::string name;
    std::vector<std::string> args;
    bool is_operator;
    uint32_t precedence; // If this is a binary op.

public:
    Prototype_AST(const std::string& name, std::vector<std::string> args,
                  bool is_operator = false, uint32_t prec = 0)
        : name(name), args(std::move(args)), is_operator(is_operator),
          precedence(prec) {}

    Function* codegen();
    const std::string& get_name() const { return name; }

    bool is_unary_op() const { return is_operator && args.size() == 1; }
    bool is_binary_op() const { return is_operator && args.size() == 2; }

    char get_operator_name() const {
        assert(is_unary_op() || is_binary_op());
        return name[name.size() - 1];
    }

    uint32_t get_binary_precedence() const { return precedence; }
};

// Function_AST - This class represents a function definition itself.
class Function_AST {
    std::unique_ptr<Prototype_AST> proto;
    std::unique_ptr<Expr_AST> body;

public:
    Function_AST(std::unique_ptr<Prototype_AST> proto,
                 std::unique_ptr<Expr_AST> body)
        : proto(std::move(proto)), body(std::move(body)) {}
    Function* codegen();
};

// Var_expr_AST - Expression class for var/in.
class Var_expr_AST : public Expr_AST {
    std::vector<std::pair<std::string, std::unique_ptr<Expr_AST>>> var_names;
    std::unique_ptr<Expr_AST> body;

public:
    Var_expr_AST(std::vector<std::pair<std::string, std::unique_ptr<Expr_AST>>> var_names,
                 std::unique_ptr<Expr_AST> body)
        : var_names(std::move(var_names)), body(std::move(body)) {}

    Value* codegen() override;
};

#endif // __TREE_H
