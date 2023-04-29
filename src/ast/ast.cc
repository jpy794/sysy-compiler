#include "ast.hh"
#include "err.hh"
#include "raw_ast.hh"

#include <any>
#include <iostream>
#include <map>

using namespace std;
using namespace ast;

/* build ast from raw ast
   to share the same visitor interface with ast,
   const_cast is used on nodes that are needed to be modified */
class ASTBuilder : public ASTVisitor {
  private:
    // FIXME: use a stack for scope
    map<string, const VarDefStmt *> _const_table;

    /* depth: wrapped by how many brackets
       off: sequence number of current exp in current bracket (starts from 0) */
    void _pack_initval(RawVarDefStmt::InitList &init, size_t depth, size_t off,
                       const vector<size_t> &dims,
                       vector<optional<Ptr<Expr>>> &res);
    PtrList<VarDefStmt> _split_vardef(RawVarDefStmt &raw_vardef);

  public:
    using ASTVisitor::visit;
    any visit(const Root &node) final;
    /* stmt */
    any visit(const BlockStmt &node) final;
    any visit(const IfStmt &node) final;
    any visit(const WhileStmt &node) final;
    any visit(const BreakStmt &node) final { throw unreachable_error{}; }
    any visit(const ContinueStmt &node) final { throw unreachable_error{}; }
    any visit(const ReturnStmt &node) final { throw unreachable_error{}; }
    any visit(const AssignStmt &node) final { throw unreachable_error{}; }
    any visit(const ExprStmt &node) final { throw unreachable_error{}; }
    /* expr */
    any visit(const CallExpr &node) final;
    any visit(const LiteralExpr &node) final;
    any visit(const LValExpr &node) final;
    any visit(const BinaryExpr &node) final;
    any visit(const UnaryExpr &node) final;
    /* raw */
    any visit(const RawVarDefGlobal &node) final { throw unreachable_error{}; }
    any visit(const RawVarDefStmt &node) final { throw unreachable_error{}; }
    any visit(const RawFunDefGlobal &node) final { throw unreachable_error{}; }
    /* unexpected nodes in raw ast */
    any visit(const VarDefStmt &node) final {
        throw logic_error{"unexpected node in raw ast"};
    }
    any visit(const FunDefGlobal &node) final {
        throw logic_error{"unexpected node in raw ast"};
    }
    any visit(const VarDefGlobal &node) final {
        throw logic_error{"unexpected node in raw ast"};
    }
};

/* expr node
    if expr is literal or const lval
        return literal
    else if binary or unary exp (except for NOT) and both operands are literals
        return literal result of the operation
    else return expr
*/

struct ExprLiteral {
    bool is_const{false};
    variant<float, int> val;
    ExprLiteral() = default;
    ExprLiteral(float v) : is_const(true), val(v) {}
    ExprLiteral(int v) : is_const(true), val(v) {}
    ExprLiteral(variant<float, int> v) : is_const(true), val(v) {}
};

any ASTBuilder::visit(const CallExpr &node) { return ExprLiteral{}; }
any ASTBuilder::visit(const LiteralExpr &node) { return ExprLiteral{node.val}; }

template <typename K, typename V>
bool contains(const std::map<K, V> &map, const K &key) {
    return map.find(key) != map.end();
}

any ASTBuilder::visit(const LValExpr &node) {
    if (not contains(_const_table, node.var_name)) {
        return ExprLiteral{};
    }
    auto &vardef = _const_table[node.var_name];
    vector<int> idxs;
    for (auto &pidx : node.idxs) {
        auto expr_literal = any_cast<ExprLiteral>(visit(*pidx));
        if (not expr_literal.is_const) {
            // if any of index is not const, lval is not const
            return ExprLiteral{};
        }
        if (not holds_alternative<int>(expr_literal.val)) {
            throw runtime_error{"type of a const index is not int"};
        }
        auto val = get<int>(expr_literal.val);
        if (val < 0) {
            throw runtime_error{"const index < 0"};
        }
        idxs.push_back(val);
    }
    if (vardef->dims.size() != idxs.size()) {
        throw runtime_error{"size of idxs is not the same as defined"};
    }
    // base type
    auto off = 0;

    // array type
    for (size_t i = 0; i < vardef->dims.size(); i++) {
        off += vardef->dims[i] * idxs[i];
    }

    if (not vardef->init_vals[off].has_value()) {
        throw logic_error{"an item of const_table should be fully initialized"};
    }
    auto init_expr = visit(*(vardef->init_vals[off].value()));
    auto init_literal = any_cast<ExprLiteral>(init_expr);
    return init_literal;
}

template <typename A, typename B>
auto bin_op(A lhs, B rhs, BinOp op) -> decltype(lhs + rhs) {
    decltype(lhs + rhs) res;
    switch (op) {
    case BinOp::ADD:
        res = lhs + rhs;
        break;
    case BinOp::SUB:
        res = lhs - rhs;
        break;
    case BinOp::MUL:
        res = lhs * rhs;
        break;
    case BinOp::DIV:
        res = lhs / rhs;
        break;
    case BinOp::MOD:
        if constexpr (is_same_v<int, A> && is_same_v<int, B>) {
            res = lhs % rhs;
            break;
        } else {
            throw runtime_error{"only allow int mod int"};
        }
    default:
        throw runtime_error{"only add exp is allowed in const exp"};
    }
    return res;
}

any ASTBuilder::visit(const BinaryExpr &node) {
    auto lhs_literal = any_cast<ExprLiteral>(visit(*node.lhs));
    auto rhs_literal = any_cast<ExprLiteral>(visit(*node.rhs));
    if (lhs_literal.is_const && rhs_literal.is_const) {
        bool lhs_int = holds_alternative<int>(lhs_literal.val);
        bool rhs_int = holds_alternative<int>(rhs_literal.val);
        if (lhs_int && rhs_int) {
            auto lhs = get<int>(lhs_literal.val);
            auto rhs = get<int>(rhs_literal.val);
            // return int
            return ExprLiteral{bin_op(lhs, rhs, node.op)};
        } else if (lhs_int && !rhs_int) {
            auto lhs = get<int>(lhs_literal.val);
            auto rhs = get<float>(rhs_literal.val);
            return ExprLiteral{bin_op(lhs, rhs, node.op)};
        } else if (!lhs_int && rhs_int) {
            auto lhs = get<float>(lhs_literal.val);
            auto rhs = get<int>(rhs_literal.val);
            return ExprLiteral{bin_op(lhs, rhs, node.op)};
        } else {
            auto lhs = get<float>(lhs_literal.val);
            auto rhs = get<float>(rhs_literal.val);
            return ExprLiteral{bin_op(lhs, rhs, node.op)};
        }
    }
    return ExprLiteral{};
}

template <typename B> auto unary_op(B rhs, UnaryOp op) -> decltype(rhs) {
    decltype(rhs) res;
    switch (op) {
    case UnaryOp::PlUS:
        res = rhs;
        break;
    case UnaryOp::MINUS:
        res = -rhs;
        break;
    default:
        throw runtime_error{"not is not allowed except for cond exp"};
    }
    return res;
}

any ASTBuilder::visit(const UnaryExpr &node) {
    auto rhs_literal = any_cast<ExprLiteral>(visit(*node.rhs));
    if (rhs_literal.is_const) {
        bool rhs_int = holds_alternative<int>(rhs_literal.val);
        if (rhs_int) {
            auto rhs = get<int>(rhs_literal.val);
            return ExprLiteral{unary_op(rhs, node.op)};
        } else {
            auto rhs = get<float>(rhs_literal.val);
            return ExprLiteral{unary_op(rhs, node.op)};
        }
    }
    return ExprLiteral{};
}

/* stmt and global node
    for nodes that could contains raw vardefstmt
        replace the raw ast one with multiple ast ones
        for each ast vardef node
            evaluate dims
            serialize init_val
            if is_const
                replace expr in init_val with literal expr node

    if blockstmt or function
        adjust scope
*/

template <typename Derived, typename Base> bool is(Base *ptr) {
    return dynamic_cast<Derived *>(ptr) != nullptr;
}

void ASTBuilder::_pack_initval(RawVarDefStmt::InitList &init, size_t depth,
                               size_t off, const vector<size_t> &dims,
                               vector<optional<Ptr<Expr>>> &res) {
    auto sub_dim_len{1};
    for (auto i = depth + 1; i < dims.size(); i++) {
        sub_dim_len *= dims[i];
    }
    auto dim_len = dims[depth] * sub_dim_len;
    auto dim_idx_begin = off * dim_len;
    if (holds_alternative<Ptr<Expr>>(init.val)) {
        // this is a value, not a init list
        // TODO: evaluate the value for const vardef
        auto &expr = get<Ptr<Expr>>(init.val);
        res[dim_idx_begin] = Ptr<Expr>{};
        res[dim_idx_begin]->swap(expr);
    }
    // this is a init list
    auto &list = get<PtrList<RawVarDefStmt::InitList>>(init.val);
    for (size_t i = 0; i < list.size(); i++) {
        _pack_initval(*list[i], depth + 1, i, dims, res);
    }
}

int expect_const_pos_int(const ExprLiteral &literal) {
    if (not literal.is_const) {
        throw runtime_error{"literal is not a compile-time const"};
    }
    if (not holds_alternative<int>(literal.val)) {
        throw runtime_error{"literal is not a int"};
    }
    auto val = get<int>(literal.val);
    if (val < 0) {
        throw runtime_error{"literal < 0"};
    }
    return val;
}

PtrList<VarDefStmt> ASTBuilder::_split_vardef(RawVarDefStmt &raw_vardef) {
    PtrList<VarDefStmt> ret;
    for (auto &&entry : raw_vardef.var_defs) {
        auto vardef = new VarDefStmt;
        vardef->var_name = entry->var_name;
        vardef->type = entry->type;
        vardef->is_const = entry->is_const;
        for (auto &&dim : entry->dims) {
            auto literal = any_cast<ExprLiteral>(visit(*dim));
            vardef->dims.push_back(expect_const_pos_int(literal));
        }

        size_t len{1};
        for (auto i : vardef->dims) {
            len *= i;
        }
        vardef->init_vals.resize(len);
        if (entry->init_list.has_value()) {
            _pack_initval(*entry->init_list.value(), 0, 0, vardef->dims,
                          vardef->init_vals);
        }
        ret.push_back(Ptr<VarDefStmt>{vardef});
    }
    return ret;
}

any ASTBuilder::visit(const Root &node) {
    auto &n = const_cast<Root &>(node);
    for (auto it = n.globals.begin(); it != n.globals.end();) {
        if (is<FunDefGlobal>(it->get())) {
            auto &raw = dynamic_cast<RawFunDefGlobal &>(*(it->get()));
            auto fundef = new FunDefGlobal;
            fundef->body.swap(raw.body);
            fundef->fun_name = raw.fun_name;
            fundef->ret_type = raw.ret_type;
            // parse params
            for (auto &raw_param : raw.params) {
                FunDefGlobal::Param param;
                param.type = raw_param->type;
                if (raw_param->is_ptr) {
                    // pointer param need a 0 as dim[0]
                    param.dims.push_back(0);
                }
                for (auto &raw_dim : raw_param->dims) {
                    auto literal = any_cast<ExprLiteral>(visit(*raw_dim));
                    auto val = expect_const_pos_int(literal);
                    param.dims.push_back(val);
                }
                fundef->params.push_back(param);
            }
            it = n.globals.erase(it);
            n.globals.emplace(it, fundef);
        } else /* is VarDefGlobal */ {
            auto &raw_global = dynamic_cast<RawVarDefGlobal &>(*(it->get()));
            auto &raw_stmt = *raw_global.vardef_stmt;
            auto vardefs = _split_vardef(raw_stmt);
            it = n.globals.erase(it);
            for (auto &def : vardefs) {
                auto global = new VarDefGlobal;
                global->vardef_stmt.swap(def);
                n.globals.emplace(it, global);
            }
        }
    }
    return {};
}

AST::AST(const string &src_file) { std::cout << "hello AST\n"; }
