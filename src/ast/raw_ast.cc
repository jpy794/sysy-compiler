#include "raw_ast.hh"
#include "err.hh"
#include "sysyVisitor.h"

using namespace std;
using namespace ast;

// FIXME: c++17 doesn't support 'using enum'
// using enum BaseType;

class RawASTBuilder : public sysyVisitor {
    any visitCompUnit(sysyParser::CompUnitContext *ctx) final;
    any visitVardecl(sysyParser::VardeclContext *ctx) final;
    any visitVardef(sysyParser::VardefContext *ctx) final;
    any visitVarInit(sysyParser::VarInitContext *ctx) final;
    any visitFuncdef(sysyParser::FuncdefContext *ctx) final;
    any visitFuncparam(sysyParser::FuncparamContext *ctx) final;
    any visitBlock(sysyParser::BlockContext *ctx) final;
    any visitExpStmt(sysyParser::ExpStmtContext *ctx) final;
    any visitStmt(sysyParser::StmtContext *ctx) final;
    any visitLval(sysyParser::LvalContext *ctx) final;
    any visitFuncCall(sysyParser::FuncCallContext *ctx) final;
    any visitParenExp(sysyParser::ParenExpContext *ctx) final;
    any visitExp(sysyParser::ExpContext *ctx) final;
    any visitNumber(sysyParser::NumberContext *ctx) final;
};

/* we can not return unique_ptr directly
   as std::any seems to require T to be copy constructable */
template <typename T> T *as_raw_ptr(const any &rhs) {
    auto raw_ptr = any_cast<T *>(rhs);
    if (raw_ptr == nullptr) {
        throw logic_error{"trying to as_raw_ptr(nullptr)"};
    }
    return raw_ptr;
}

template <typename T> Ptr<T> as_ptr(const any &rhs) {
    return Ptr<T>(as_raw_ptr<T>(rhs));
}

RawAST RawAST::parse_sysy_src(const string &src) { return {}; }

any RawASTBuilder::visitExp(sysyParser::ExpContext *ctx) {
    Expr *ret{nullptr};

    if (ctx->exp().size() == 1) {
        // unary
        auto node = new UnaryExpr;
        node->rhs = as_ptr<Expr>(visit(ctx->exp()[0]));
        if (ctx->Not()) {
            node->op = UnaryOp::NOT;
        } else if (ctx->Plus()) {
            node->op = UnaryOp::PlUS;
        } else if (ctx->Minus()) {
            node->op = UnaryOp::MINUS;
        } else {
            throw unreachable_error{};
        }
        ret = node;
    } else if (ctx->exp().size() == 2) {
        // binary
        auto node = new BinaryExpr;
        node->lhs = as_ptr<Expr>(visit(ctx->exp()[0]));
        node->rhs = as_ptr<Expr>(visit(ctx->exp()[1]));
        if (ctx->Plus()) {
            node->op = BinOp::ADD;
        } else if (ctx->Minus()) {
            node->op = BinOp::SUB;
        } else if (ctx->Multiply()) {
            node->op = BinOp::MUL;
        } else if (ctx->Divide()) {
            node->op = BinOp::DIV;
        } else if (ctx->Modulo()) {
            node->op = BinOp::MOD;
        } else if (ctx->Less()) {
            node->op = BinOp::LT;
        } else if (ctx->LessEqual()) {
            node->op = BinOp::LE;
        } else if (ctx->Greater()) {
            node->op = BinOp::GT;
        } else if (ctx->GreaterEqual()) {
            node->op = BinOp::GE;
        } else if (ctx->Equal()) {
            node->op = BinOp::EQ;
        } else if (ctx->NonEqual()) {
            node->op = BinOp::NE;
        } else if (ctx->And()) {
            node->op = BinOp::AND;
        } else if (ctx->Or()) {
            node->op = BinOp::OR;
        } else {
            throw unreachable_error{};
        }
        ret = node;
    } else if (ctx->parenExp() || ctx->lval() || ctx->number() ||
               ctx->funcCall()) {
        // pass through
        ret = as_raw_ptr<Expr>(visit(ctx->parenExp()));
    } else {
        throw unreachable_error{};
    }

    return ret;
}

any RawASTBuilder::visitNumber(sysyParser::NumberContext *ctx) {
    Expr *ret{nullptr};
    auto node = new LiteralExpr;

    if (ctx->FloatConst()) {
        auto val_str = ctx->FloatConst()->getText();
        node->val = stof(val_str);
        node->type = BaseType::FLOAT;
    } else if (ctx->IntConst()) {
        auto val_str = ctx->IntConst()->getText();
        node->val = stoi(val_str);
        node->type = BaseType::INT;
    } else {
        throw unreachable_error{};
    }

    ret = node;
    return ret;
}

any RawASTBuilder::visitParenExp(sysyParser::ParenExpContext *ctx) {
    Expr *ret{nullptr};
    ret = as_raw_ptr<Expr>(visit(ctx->exp()));
    return ret;
}

any RawASTBuilder::visitFuncCall(sysyParser::FuncCallContext *ctx) {
    Expr *ret{nullptr};
    auto node = new CallExpr;

    node->fun_name = ctx->Identifier()->getText();
    for (auto &&pexp : ctx->exp()) {
        node->args.push_back(as_ptr<Expr>(visit(pexp)));
    }

    return ret;
}

any RawASTBuilder::visitLval(sysyParser::LvalContext *ctx) {
    Expr *ret{nullptr};
    auto node = new LValExpr;

    node->var_name = ctx->Identifier()->getText();
    for (auto &&pexp : ctx->exp()) {
        node->idxs.push_back(as_ptr<Expr>(visit(pexp)));
    }

    ret = node;
    return ret;
}
