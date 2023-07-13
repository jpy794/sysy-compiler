#pragma once

#include "ast.hh"
#include "err.hh"

#include <any>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace ast {

using std::string;

#define any_string(x) (std::any_cast<string>(x))
#define no_trailing(x) (x.substr(0, x.size() - 1))

class ASTPrinter : public ASTVisitor {
    // SurroundType
    enum class STP { Void, Object, Array, String };

  private:
    inline static string surround(const string &s, STP st) {
        switch (st) {
        case STP::Object:
            return "{" + s + "}";
        case STP::Array:
            return "[" + s + "]";
        case STP::String:
            return "\"" + s + "\"";
        case STP::Void:
            return s;
        default:
            throw std::runtime_error("unexpected cases");
        }
    }

    inline static string kvpair(const string &k, const string &v,
                                STP vst = STP::Void) {
        return surround(k, STP::String) + ":" + surround(v, vst);
    }
    inline static string Object(const string &s) {
        return surround(s, STP::Object);
    }
    inline static string Array(const string &s) {
        return surround(s, STP::Array);
    }

    template <typename T> string visitArrayLike(const T &array) {
        string value{};
        for (auto &elem : array) {
            value += any_string(elem->accept(*this)) + ",";
        }
        return Array(no_trailing(value));
    }

    string dims2array(const std::vector<size_t> &dims) {
        string diminfo;
        for (auto dim : dims)
            diminfo += std::to_string(dim) + ",";
        return Array(no_trailing(diminfo));
    }

    string make_return(const string &name, const string &body) {
        return Object(kvpair(name, Object(body)));
    }

    std::string BaseTypeStr(const enum BaseType &type) {
        switch (type) {
        case BaseType::VOID:
            return "void";
            break;
        case BaseType::FLOAT:
            return "float";
            break;
        case BaseType::INT:
            return "int";
            break;
        }
        throw std::runtime_error("unexpected cases");
    }

    std::string BinOpStr(const enum BinOp &type) {
        switch (type) {
        case BinOp::ADD:
            return "+";
            break;
        case BinOp::SUB:
            return "-";
            break;
        case BinOp::MUL:
            return "*";
            break;
        case BinOp::DIV:
            return "/";
            break;
        case BinOp::MOD:
            return "%";
            break;
        case BinOp::LT:
            return "<";
            break;
        case BinOp::GT:
            return ">";
            break;
        case BinOp::LE:
            return "<=";
            break;
        case BinOp::GE:
            return ">=";
            break;
        case BinOp::EQ:
            return "==";
            break;
        case BinOp::NE:
            return "!=";
            break;
        case BinOp::AND:
            return "&&";
            break;
        case BinOp::OR:
            return "||";
            break;
        }
        throw std::runtime_error("unexpected cases");
    }

    std::string UnaryOpStr(const enum UnaryOp &type) {
        switch (type) {
        case UnaryOp::PlUS:
            return "+";
            break;
        case UnaryOp::MINUS:
            return "-";
            break;
        case UnaryOp::NOT:
            return "!";
            break;
        }
        throw std::runtime_error("unexpected cases");
    }

  public:
    virtual std::any visit(const Root &node) override final;
    /* global */
    virtual std::any visit(const FunDefGlobal &node) override final;
    virtual std::any visit(const VarDefGlobal &node) override final;
    /* stmt */
    virtual std::any visit(const BlockStmt &node) override final;
    virtual std::any visit(const IfStmt &node) override final;
    virtual std::any visit(const WhileStmt &node) override final;
    virtual std::any visit(const BreakStmt &node) override final;
    virtual std::any visit(const ContinueStmt &node) override final;
    virtual std::any visit(const ReturnStmt &node) override final;
    virtual std::any visit(const AssignStmt &node) override final;
    virtual std::any visit(const VarDefStmt &node) override final;
    virtual std::any visit(const ExprStmt &node) override final;
    /* expr */
    virtual std::any visit(const CallExpr &node) override final;
    virtual std::any visit(const LiteralExpr &node) override final;
    virtual std::any visit(const LValExpr &node) override final;
    virtual std::any visit(const BinaryExpr &node) override final;
    virtual std::any visit(const UnaryExpr &node) override final;
};

} // namespace ast
