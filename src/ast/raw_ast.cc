#include "raw_ast.hh"
#include "err.hh"
#include "sysyLexer.h"
#include "sysyVisitor.h"
#include <fstream>

using namespace std;
using namespace ast;

// FIXME: c++17 doesn't support 'using enum'
// using enum BaseType;

class RawASTBuilder : public sysyVisitor {
    any visitCompUnit(sysyParser::CompUnitContext *ctx) final;
    any visitGlobalDef(sysyParser::GlobalDefContext *ctx) final;
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
        throw logic_error{"as_raw_ptr results in nullptr"};
    }
    return raw_ptr;
}

template <typename T> Ptr<T> as_ptr(const any &rhs) {
    return Ptr<T>(as_raw_ptr<T>(rhs));
}

template <typename Derived, typename Base> Ptr<Derived> cast_ptr(Base *ptr) {
    auto raw_ptr = dynamic_cast<Derived *>(ptr);
    if (not raw_ptr) {
        throw logic_error{"bad cast_ptr"};
    }
    return Ptr<Derived>(raw_ptr);
}

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
    } else if (ctx->parenExp()) {
        // pass through
        ret = as_raw_ptr<Expr>(visit(ctx->parenExp()));
    } else if (ctx->lval()) {
        ret = as_raw_ptr<Expr>(visit(ctx->lval()));
    } else if (ctx->number()) {
        ret = as_raw_ptr<Expr>(visit(ctx->number()));
    } else if (ctx->funcCall()) {
        ret = as_raw_ptr<Expr>(visit(ctx->funcCall()));
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
        node->val = stoi(val_str, 0, 0);
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

    ret = node;
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

any RawASTBuilder::visitStmt(sysyParser::StmtContext *ctx) {
    Stmt *ret{nullptr};

    if (ctx->Assign()) {
        // retrieve var_name and idxs from lval
        auto node = new AssignStmt;
        auto expr = as_raw_ptr<Expr>(visit(ctx->lval()));
        auto lval = cast_ptr<LValExpr>(expr);
        node->var_name = std::move(lval->var_name);
        node->idxs = std::move(lval->idxs);
        node->val = as_ptr<Expr>(visit(ctx->exp()));
        ret = node;
    } else if (ctx->If()) {
        auto node = new IfStmt;
        node->cond = as_ptr<Expr>(visit(ctx->exp()));
        node->then_body = as_ptr<Stmt>(visit(ctx->stmt()[0]));
        if (ctx->Else()) {
            node->else_body = as_ptr<Stmt>(visit(ctx->stmt()[1]));
        }
        ret = node;
    } else if (ctx->While()) {
        auto node = new WhileStmt;
        node->cond = as_ptr<Expr>(visit(ctx->exp()));
        node->body = as_ptr<Stmt>(visit(ctx->stmt()[0]));
        ret = node;
    } else if (ctx->Break()) {
        auto node = new BreakStmt;
        ret = node;
    } else if (ctx->Continue()) {
        auto node = new ContinueStmt;
        ret = node;
    } else if (ctx->Return()) {
        auto node = new ReturnStmt;
        if (ctx->exp()) {
            node->ret_val = as_ptr<Expr>(visit(ctx->exp()));
        }
        ret = node;
    } else if (ctx->expStmt()) {
        ret = as_raw_ptr<Stmt>(visit(ctx->expStmt()));
    } else if (ctx->block()) {
        ret = as_raw_ptr<Stmt>(visit(ctx->block()));
    } else if (ctx->vardecl()) {
        ret = as_raw_ptr<Stmt>(visit(ctx->vardecl()));
    } else {
        throw unreachable_error{};
    }

    return ret;
}

any RawASTBuilder::visitBlock(sysyParser::BlockContext *ctx) {
    Stmt *ret{nullptr};
    auto node = new BlockStmt;

    for (auto &&pstmt : ctx->stmt()) {
        node->stmts.push_back(as_ptr<Stmt>(visit(pstmt)));
    }

    ret = node;
    return ret;
}

any RawASTBuilder::visitExpStmt(sysyParser::ExpStmtContext *ctx) {
    Stmt *ret{nullptr};
    auto node = new ExprStmt;

    if (ctx->exp()) {
        node->expr = as_ptr<Expr>(visit(ctx->exp()));
    }

    ret = node;
    return ret;
}

any RawASTBuilder::visitVardecl(sysyParser::VardeclContext *ctx) {
    Stmt *ret{nullptr};

    bool is_const{false};
    if (ctx->Const()) {
        is_const = true;
    }

    BaseType var_type{BaseType::VOID};
    if (ctx->Float()) {
        var_type = BaseType::FLOAT;
    } else if (ctx->Int()) {
        var_type = BaseType::INT;
    } else {
        throw unreachable_error{};
    }

    auto node = new RawVarDefStmt;
    for (auto &&pvardef : ctx->vardef()) {
        auto entry = as_ptr<RawVarDefStmt::Entry>(visit(pvardef));
        entry->is_const = is_const;
        entry->type = var_type;
        node->var_defs.push_back(std::move(entry));
    }

    ret = node;
    return ret;
}

any RawASTBuilder::visitVardef(sysyParser::VardefContext *ctx) {
    auto entry = new RawVarDefStmt::Entry;
    entry->var_name = ctx->Identifier()->getText();
    for (auto &&pexp : ctx->exp()) {
        entry->dims.push_back(as_ptr<Expr>(visit(pexp)));
    }
    if (ctx->varInit()) {
        entry->init_list =
            as_ptr<RawVarDefStmt::InitList>(visit(ctx->varInit()));
    }
    return entry;
}

any RawASTBuilder::visitVarInit(sysyParser::VarInitContext *ctx) {
    auto ret = new RawVarDefStmt::InitList;
    if (ctx->exp()) {
        ret->is_zero_list = false;
        ret->val = as_ptr<Expr>(visit(ctx->exp()));
    } else if (ctx->varInit().size() > 0) {
        PtrList<RawVarDefStmt::InitList> list;
        for (auto &&pinit : ctx->varInit()) {
            list.push_back(as_ptr<RawVarDefStmt::InitList>(visit(pinit)));
        }
        ret->is_zero_list = false;
        ret->val = std::move(list);
    } else if (ctx->LeftBrace() && ctx->RightBrace()) {
        // zero init
        ret->is_zero_list = true;
    } else {
        throw unreachable_error{};
    }
    return ret;
}

any RawASTBuilder::visitFuncdef(sysyParser::FuncdefContext *ctx) {
    Global *ret{nullptr};
    auto node = new RawFunDefGlobal;

    if (ctx->Float()) {
        node->ret_type = BaseType::FLOAT;
    } else if (ctx->Int()) {
        node->ret_type = BaseType::INT;
    } else if (ctx->Void()) {
        node->ret_type = BaseType::VOID;
    } else {
        throw unreachable_error{};
    }
    node->fun_name = ctx->Identifier()->getText();
    node->body = cast_ptr<BlockStmt>(as_raw_ptr<Stmt>(visit(ctx->block())));
    for (auto &&pparam : ctx->funcparam()) {
        node->params.push_back(as_ptr<RawFunDefGlobal::Param>(visit(pparam)));
    }

    ret = node;
    return ret;
}

any RawASTBuilder::visitFuncparam(sysyParser::FuncparamContext *ctx) {
    auto ret = new RawFunDefGlobal::Param;

    if (ctx->Float()) {
        ret->type = BaseType::FLOAT;
    } else if (ctx->Int()) {
        ret->type = BaseType::INT;
    } else {
        throw unreachable_error{};
    }
    ret->name = ctx->Identifier()->getText();
    if (ctx->LeftBracket().size() > 0) {
        ret->is_ptr = true;
    } else {
        ret->is_ptr = false;
    }
    for (auto &&pexp : ctx->exp()) {
        ret->dims.push_back(as_ptr<Expr>(visit(pexp)));
    }

    return ret;
}

any RawASTBuilder::visitCompUnit(sysyParser::CompUnitContext *ctx) {
    auto ret = new Root;

    for (auto &&pglobal : ctx->globalDef()) {
        ret->globals.push_back(as_ptr<Global>(visit(pglobal)));
    }

    return ret;
}

any RawASTBuilder::visitGlobalDef(sysyParser::GlobalDefContext *ctx) {
    Global *ret{nullptr};

    if (ctx->funcdef()) {
        ret = as_raw_ptr<Global>(visit(ctx->funcdef()));
    } else if (ctx->vardecl()) {
        auto node = new RawVarDefGlobal;
        node->vardef_stmt =
            cast_ptr<RawVarDefStmt>(as_raw_ptr<Stmt>(visit(ctx->vardecl())));
        ret = node;
    } else {
        throw unreachable_error{};
    }

    return ret;
}

RawAST::RawAST(const string &src) {
    ifstream src_s{src};

    antlr4::ANTLRInputStream input_s{src_s};
    sysyLexer lexer{&input_s};
    antlr4::CommonTokenStream token_s{&lexer};
    sysyParser parser{&token_s};

    RawASTBuilder builder;
    root = as_ptr<Root>(builder.visit(parser.compUnit()));
}
