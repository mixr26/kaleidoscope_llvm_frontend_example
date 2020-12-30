#ifndef __TREE_H
#define __TREE_H

#include <memory>
#include <vector>

// ExprAST - Base class for all expr nodes.
class Expr_AST {
public:
    virtual ~Expr_AST() {}
};

// Number_expr_AST - Expression class for numeric literals like "1.0".
class Number_expr_AST : public Expr_AST {
    double val;

public:
    Number_expr_AST(double val) : val(val) {}
};

// Variable_expr_AST - Expression class for referencing a variable, like "a".
class Variable_expr_AST : public Expr_AST {
    std::string name;

public:
    Variable_expr_AST(const std::string& name) : name(name) {}
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
};

// Call_expr_AST - Expression class for function calls.
class Call_expr_AST : public Expr_AST {
    std::string callee;
    std::vector<std::unique_ptr<Expr_AST>> args;

public:
    Call_expr_AST(const std::string& callee,
                  std::vector<std::unique_ptr<Expr_AST>> args)
        : callee(callee), args(std::move(args)) {}
};

// Prototype_AST - this class represents the "prototype" for a function,
// which captures its name, and its argument names (thus implicitly the
// number of arguments the function takes).
class Prototype_AST {
    std::string name;
    std::vector<std::string> args;

public:
    Prototype_AST(const std::string& name, std::vector<std::string> args)
        : name(name), args(std::move(args)) {}

    const std::string& get_name() const { return name; }
};

// Function_AST - This class represents a function definition itself.
class Function_AST {
    std::unique_ptr<Prototype_AST> proto;
    std::unique_ptr<Expr_AST> body;

public:
    Function_AST(std::unique_ptr<Prototype_AST> proto,
                 std::unique_ptr<Expr_AST> body)
        : proto(std::move(proto)), body(std::move(body)) {}
};

#endif // __TREE_H
