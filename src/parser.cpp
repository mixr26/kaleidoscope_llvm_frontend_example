#include <cassert>
#include <cstdio>
#include <memory>

#include "lexer.h"
#include "parser.h"

// cur_tok/get_next_token - Provide a simple token buffer. cur_tok is the
// current token the parser is looking a. get_next_token reads another token
// from the lexer and updates cur_tok with its results.
int cur_tok;

static std::unique_ptr<Expr_AST> parse_expression();

// binop_precedence - This holds the precedence for each binary operator that is
// defined.
static std::map<char, int> binop_precedence{
    std::pair<char, int>('<', 10),
    std::pair<char, int>('+', 20),
    std::pair<char, int>('-', 20),
    std::pair<char, int>('*', 40),
};

int get_next_token() {
    return cur_tok = gettok();
}

// log_error* - There are little helper functions for error handling.
static std::unique_ptr<Expr_AST> log_error(const char* str) {
    fprintf(stderr, "log_error: %s\n", str);
    return nullptr;
}

static std::unique_ptr<Prototype_AST> log_error_p(const char* str) {
    log_error(str);
    return nullptr;
}

// numberexpr ::= number
static std::unique_ptr<Expr_AST> parse_number_expr() {
    auto result = std::make_unique<Number_expr_AST>(num_val);
    get_next_token();
    return std::move(result);
}

// parenexpr ::= '(' expression ')'
static std::unique_ptr<Expr_AST> parse_paren_expr() {
    get_next_token(); // Eat '('.
    auto v = parse_expression();
    if (!v)
        return nullptr;

    if (cur_tok != ')')
        return log_error("expected ')'.");
    get_next_token();
    return v;
}

// identifierexpr
//      ::= identifier
//      ::= identifier '(' expression* ')'
static std::unique_ptr<Expr_AST> parse_identifier_expr() {
    std::string id_name = identifier_str;

    get_next_token(); // Eat identifier.

    if (cur_tok != '(') // Simple variable ref.
        return std::make_unique<Variable_expr_AST>(id_name);

    // Call.
    get_next_token(); // Eat '('.
    std::vector<std::unique_ptr<Expr_AST>> args;
    if (cur_tok != ')') {
        while (true) {
            if (auto arg = parse_expression())
                args.emplace_back(std::move(arg));
            else
                return nullptr;

            if (cur_tok == ')')
                break;

            if (cur_tok != ',')
                return log_error("Expected ')' or ',' in argument list.");
            get_next_token();
        }
    }

    // Eat ')'.
    get_next_token();

    return std::make_unique<Call_expr_AST>(id_name, std::move(args));
}

// ifexpr ::= 'if' expression 'then' expression 'else' expression
static std::unique_ptr<Expr_AST> parse_if_expr() {
    get_next_token(); // Eat the 'if'.

    // Condition.
    auto cond = parse_expression();
    if (!cond)
        return nullptr;

    if (cur_tok != static_cast<int>(Token::TOK_THEN))
        return log_error("Expected then.");
    get_next_token(); // Eat the 'then'.

    auto then = parse_expression();
    if (!then)
        return nullptr;

    if (cur_tok != static_cast<int>(Token::TOK_ELSE))
        return log_error("Expected else.");
    get_next_token(); // Eat the else.

    auto elze = parse_expression();
    if (!elze)
        return nullptr;

    return std::make_unique<If_expr_AST>(std::move(cond), std::move(then),
                                         std::move(elze));
}

// forexpr ::= 'for' indentifier '=' expr (',' expr)? 'in' expression
static std::unique_ptr<Expr_AST> parse_for_expr() {
    get_next_token(); // Eat the 'for'.

    if (cur_tok != static_cast<int>(Token::TOK_IDENTIFIER))
        return log_error("Expected identifier after for.");

    std::string id_name = identifier_str;
    get_next_token(); // Eat the identifier.

    if (cur_tok != '=')
        return log_error("Expected '=' after 'for'.");
    get_next_token(); // Eat the '='.

    auto start = parse_expression();
    if (!start)
        return nullptr;
    if (cur_tok != ',')
        return log_error("Expected ',' after for start value.");
    get_next_token();

    auto end = parse_expression();
    if (!end)
        return nullptr;

    // The step value is optional.
    std::unique_ptr<Expr_AST> step;
    if (cur_tok == ',') {
        get_next_token();
        step = parse_expression();
        if (!step)
            return nullptr;
    }

    if (cur_tok != static_cast<int>(Token::TOK_IN))
        return log_error("Expected 'in' after for.");
    get_next_token(); // Eat the 'in'.

    auto body = parse_expression();
    if (!body)
        return nullptr;

    return std::make_unique<For_expr_AST>(id_name, std::move(start),
                                          std::move(end), std::move(step),
                                          std::move(body));
}

// primary
//      ::= identifierexpr
//      ::= numberexpr
//      ::= parenexpr
//      ::= ifexpr
static std::unique_ptr<Expr_AST> parse_primary() {
    switch (cur_tok) {
    default:
        return log_error("Unknown token when expecting an expression.");
    case static_cast<int>(Token::TOK_IDENTIFIER):
        return parse_identifier_expr();
    case static_cast<int>(Token::TOK_NUMBER):
        return parse_number_expr();
    case '(':
        return parse_paren_expr();
    case static_cast<int>(Token::TOK_IF):
        return parse_if_expr();
    case static_cast<int>(Token::TOK_FOR):
        return parse_for_expr();
    }
}

// get_tok_precedence - Get the precedence of the pending binary operator token.
static int get_tok_precedence() {
    if (!isascii(cur_tok))
        return -1;

    // Make sure it's a declared binop.
    int tok_prec = binop_precedence[cur_tok];
    if (tok_prec <= 0)
        return -1;
    return tok_prec;
}

// binoprhs
//      ::= ('+' primary)*
static std::unique_ptr<Expr_AST> parse_binop_rhs(int expr_prec,
                                                 std::unique_ptr<Expr_AST> lhs) {
    // If this is a binop, find its precedence.
    while (true) {
        int tok_prec = get_tok_precedence();

        // If this is a binop that binds at least as tightly as the current
        // binop, consume it, otherwise we are done.
        if (tok_prec < expr_prec)
            return lhs;

        // This is a binop.
        int bin_op = cur_tok;
        get_next_token(); // Eat binop.

        // Parse the primary expression after the binary operator.
        auto rhs = parse_primary();
        if (!rhs)
            return nullptr;

        // If bin_op binds less tightly with RHS operator than the operator
        // after RHS, let the pending operator take RHS as its LHS.
        int next_prec = get_tok_precedence();
        if (tok_prec < next_prec) {
            rhs = parse_binop_rhs(tok_prec + 1, std::move(rhs));
            if (!rhs)
                return nullptr;
        }

        // Merge LHS/RHS.
        lhs = std::make_unique<Binary_expr_AST>(bin_op, std::move(lhs),
                                                std::move(rhs));
    } // Loop around.
}

// prototype
//      ::= id '(' id* ')
static std::unique_ptr<Prototype_AST> parse_prototype() {
    if (cur_tok != static_cast<int>(Token::TOK_IDENTIFIER))
        return log_error_p("Expected function name in prototype.");

    std::string fn_name = identifier_str;
    get_next_token();

    if (cur_tok != '(')
        return log_error_p("Expected '(' in prototype.");

    // Read the list of argument names.
    std::vector<std::string> arg_names;
    while (get_next_token() == static_cast<int>(Token::TOK_IDENTIFIER))
        arg_names.push_back(identifier_str);
    if (cur_tok != ')')
        return log_error_p("Expected ')' in prototype.");

    // Success.
    get_next_token(); // Eat ')'.

    return std::make_unique<Prototype_AST>(fn_name, std::move(arg_names));
}

// definition ::= 'def' prototype exptression
std::unique_ptr<Function_AST> parse_definition() {
    get_next_token();
    auto proto = parse_prototype();
    if (!proto)
        return nullptr;

    if (auto e = parse_expression())
        return std::make_unique<Function_AST>(std::move(proto), std::move(e));
    return nullptr;
}

// external ::= 'extern' prototype
std::unique_ptr<Prototype_AST> parse_extern() {
    get_next_token(); // Eat 'extern'.
    return parse_prototype();
}

// toplevelexpr ::= expression
std::unique_ptr<Function_AST> parse_top_level_expr() {
    if (auto e = parse_expression()) {
        // Make an anonymous prototype.
        auto proto = std::make_unique<Prototype_AST>("__anon_expr", std::vector<std::string>());
        return std::make_unique<Function_AST>(std::move(proto), std::move(e));
    }
    return nullptr;
}

// expression
//      ::= primary binoprhs
static std::unique_ptr<Expr_AST> parse_expression() {
    auto lhs = parse_primary();
    if (!lhs)
        return nullptr;

    return parse_binop_rhs(0, std::move(lhs));
}
