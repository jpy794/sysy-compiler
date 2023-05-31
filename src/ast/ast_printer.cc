#include "ast_printer.hh"
#include "ast.hh"
#include "err.hh"
#include <any>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <variant>

using namespace ast;
using namespace std;

std::any ASTPrinter::visit(const Root &node) {
    string body = kvpair("globals", visitArrayLike(node.globals), STP::Void);
    return make_return("Root", body);
}

std::any ASTPrinter::visit(const FunDefGlobal &node) {
    string body{};
    body += kvpair("ret_type", BaseTypeStr(node.ret_type), STP::String) + ",";
    body += kvpair("func_name", node.fun_name, STP::String) + ",";

    string paramlist{};
    for (auto param : node.params) {
        string paraminfo{};
        paraminfo += kvpair("type", BaseTypeStr(param.type), STP::String) + ",";
        paraminfo += kvpair("name", param.name, STP::String) + ",";
        paraminfo += kvpair("dims", dims2array(param.dims));

        paramlist += Object(paraminfo) + ",";
    }
    paramlist = Array(no_trailing(paramlist));
    body += kvpair("param_list", paramlist) + ",";

    body += kvpair("body", any_string(node.body->accept(*this)));

    return make_return("FunDefGlobal", body);
}

std::any ASTPrinter::visit(const VarDefGlobal &node) {
    string body{};
    body += kvpair("vardef_stmt", any_string(node.vardef_stmt->accept(*this)));
    return make_return("VarDefGlobal", body);
}

std::any ASTPrinter::visit(const BlockStmt &node) {
    string body{};
    body += kvpair("stmts", visitArrayLike(node.stmts));
    return make_return("BlockStmt", body);
}

std::any ASTPrinter::visit(const IfStmt &node) {
    string body{};
    body += kvpair("cond", any_string(node.cond->accept(*this))) + ",";
    body += kvpair("if_true", any_string(node.then_body->accept(*this)));
    if (node.else_body.has_value()) {
        body += ",";
        body += kvpair("if_false",
                       any_string(node.else_body.value()->accept(*this)));
    }
    return make_return("IfStmt", body);
}

std::any ASTPrinter::visit(const WhileStmt &node) {
    string body{};
    body += kvpair("cond", any_string(node.cond->accept(*this))) + ",";
    body += kvpair("body", any_string(node.body->accept(*this)));
    return make_return("WhileStmt", body);
}

std::any ASTPrinter::visit(const BreakStmt &node) {
    return make_return("BreakStmt", "");
}

std::any ASTPrinter::visit(const ContinueStmt &node) {
    return make_return("ContinueStmt", "");
}

std::any ASTPrinter::visit(const ReturnStmt &node) {
    string body{};
    if (node.ret_val.has_value())
        body +=
            kvpair("ret_val", any_string(node.ret_val.value()->accept(*this)));
    return make_return("ReturnStmt", body);
}

std::any ASTPrinter::visit(const AssignStmt &node) {
    string body{};
    body += kvpair("var_name", node.var_name, STP::String) + ",";
    body += kvpair("idxs", visitArrayLike(node.idxs)) + ",";
    body += kvpair("val", any_string(node.val->accept(*this)));
    return make_return("AssignStmt", body);
}

template <typename T>
pair<size_t, size_t> _range_same_kind(size_t &pos, const VarDefStmt &node) {
    auto start = pos;
    while (pos < node.init_vals.size() and
           holds_alternative<T>(node.init_vals[pos]))
        pos++;
    return {start, pos - 1};
}

std::any ASTPrinter::visit(const VarDefStmt &node) {
    string body{};
    body += kvpair("const", node.is_const ? "true" : "false") + ",";
    body += kvpair("type", BaseTypeStr(node.type), STP::String) + ",";
    body += kvpair("var_name", node.var_name, STP::String) + ",";
    body += kvpair("dims", dims2array(node.dims)) + ",";

    string init_vals{};
    for (size_t pos = 0; pos < node.init_vals.size();) {
        auto &ini = node.init_vals[pos];
        if (holds_alternative<Ptr<Expr>>(ini)) { // explicit init
            auto &expr = get<Ptr<Expr>>(ini);
            init_vals +=
                kvpair(to_string(pos), any_string(expr->accept(*this))) + ",";
            pos++;
        } else if (holds_alternative<VarDefStmt::Implicit0>(ini)) {
            auto [l, r] = _range_same_kind<VarDefStmt::Implicit0>(pos, node);
            init_vals += kvpair(to_string(l) + "~" + to_string(r),
                                "implicit init to 0", STP::String) +
                         ",";
        } else if (holds_alternative<VarDefStmt::Undef>(ini)) {
            auto [l, r] = _range_same_kind<VarDefStmt::Undef>(pos, node);
            init_vals += kvpair(to_string(l) + "~" + to_string(r),
                                "undefined init", STP::String) +
                         ",";
        } else
            throw unreachable_error{};
    }
    init_vals = no_trailing(init_vals);
    body += kvpair("init_vals", init_vals, STP::Object);
    return make_return("VarDefStmt", body);
}

std::any ASTPrinter::visit(const ExprStmt &node) {
    string body{};
    if (node.expr.has_value())
        body += kvpair("expr", any_string(node.expr.value()->accept(*this)));
    return make_return("ExprStmt", body);
}

std::any ASTPrinter::visit(const CallExpr &node) {
    string body{};
    body += kvpair("fun_name", node.fun_name, STP::String) + ",";
    body += kvpair("args", visitArrayLike(node.args));
    return make_return("CallExpr", body);
}

std::any ASTPrinter::visit(const LiteralExpr &node) {
    string body{};
    body += kvpair("type", BaseTypeStr(node.type), STP::String) + ",";
    switch (node.type) {
    case BaseType::INT:
        body += kvpair("val", to_string(get<int>(node.val)));
        break;
    case BaseType::FLOAT:
        body += kvpair("val", to_string(get<float>(node.val)));
        break;
    default:
        throw logic_error("invalid type for LiteralExpr");
    }
    return make_return("LiteralExpr", body);
}

std::any ASTPrinter::visit(const LValExpr &node) {
    string body{};
    body += kvpair("var_name", node.var_name, STP::String) + ",";
    body += kvpair("idxs", visitArrayLike(node.idxs));
    return make_return("LValExpr", body);
}

std::any ASTPrinter::visit(const BinaryExpr &node) {
    string body{};
    body += kvpair("op", BinOpStr(node.op), STP::String) + ",";
    body += kvpair("lhs", any_string(node.lhs->accept(*this))) + ",";
    body += kvpair("rhs", any_string(node.rhs->accept(*this)));
    return make_return("BinaryExpr", body);
}

std::any ASTPrinter::visit(const UnaryExpr &node) {
    string body{};
    body += kvpair("op", UnaryOpStr(node.op), STP::String) + ",";
    body += kvpair("rhs", any_string(node.rhs->accept(*this)));
    return make_return("UnaryExpr", body);
}
