#include "astprinter.hh"
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

}
std::any ASTPrinter::visit(const VarDefStmt &node) {
}

std::any ASTPrinter::visit(const ExprStmt &node) {
}
std::any ASTPrinter::visit(const CallExpr &node) {
}
std::any ASTPrinter::visit(const LiteralExpr &node) {
}

std::any ASTPrinter::visit(const LValExpr &node) {
}

std::any ASTPrinter::visit(const BinaryExpr &node) {
}
std::any ASTPrinter::visit(const UnaryExpr &node) {
}
