#include "ast.hh"
#include "ast_printer.hh"
#include "err.hh"
#include "raw_ast.hh"

#include <map>

using namespace std;
using namespace ast;

template <typename K, typename V>
bool contains(const std::map<K, V> &map, const K &key) {
    return map.find(key) != map.end();
}

class ConstTable {
  private:
    // stack for scope
    vector<map<string, const VarDefStmt *>> _const_table;

  public:
    ConstTable() {
        // global var
        push();
    }

    const VarDefStmt *lookup(const string &name) const {
        for (auto it = _const_table.rbegin(); it != _const_table.rend(); ++it) {
            if (contains(*it, name)) {
                return it->at(name);
            }
        }
        return nullptr;
    }

    void insert(const string &name, const VarDefStmt *def) {
        _const_table.back().emplace(name, def);
    }

    void push() { _const_table.emplace_back(); }
    void pop() { _const_table.pop_back(); }
};

/* build ast from raw ast
   to share the same visitor interface with ast,
   const_cast is used on nodes that are needed to be modified */
class ASTBuilder : public ASTVisitor {
  private:
    ConstTable _const_table;

    /* depth: wrapped by how many brackets
       off: beginning index of current brace (starts from 0)
       return: the new off */
    size_t _pack_initval(RawVarDefStmt::InitList &init, size_t depth,
                         size_t off, const vector<size_t> &dims,
                         vector<optional<Ptr<Expr>>> &res, BaseType type);
    PtrList<VarDefStmt> _split_vardef(RawVarDefStmt &raw_vardef);

  public:
    using ASTVisitor::visit;
    any visit(const Root &node) final;
    /* stmt */
    any visit(const BlockStmt &node) final;
    any visit(const IfStmt &node) final;
    any visit(const WhileStmt &node) final;
    any visit(const BreakStmt &node) final { return {}; }
    any visit(const ContinueStmt &node) final { return {}; }
    any visit(const ReturnStmt &node) final { return {}; }
    any visit(const AssignStmt &node) final { return {}; }
    any visit(const ExprStmt &node) final { return {}; }
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
    else return empty literal
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

any ASTBuilder::visit(const LValExpr &node) {
    auto vardef = _const_table.lookup(node.var_name);
    if (not vardef) {
        return ExprLiteral{};
    }
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
                TODO: replace expr in init_val with literal expr node

    for nodes that could contains raw fundef
        replace the raw ast one with ast one

    for nodes that could contains stmt (for vardef)
        visit(all of its stmts)

    if blockstmt
        adjust scope
*/

template <typename Derived, typename Base> bool is(Base *ptr) {
    return dynamic_cast<Derived *>(ptr) != nullptr;
}

size_t ASTBuilder::_pack_initval(RawVarDefStmt::InitList &init, size_t depth,
                                 size_t off, const vector<size_t> &dims,
                                 vector<optional<Ptr<Expr>>> &res,
                                 BaseType type) {
    auto dim_len{1};
    for (auto i = depth; i < dims.size(); i++) {
        dim_len *= dims[i];
    }
    auto dim_idx_begin = off;
    if (init.is_zero_list) {
        // this is a zero init
        for (auto i = 0; i < dim_len; i++) {
            auto literal = new LiteralExpr{};
            literal->type = type;
            if (type == BaseType::INT) {
                literal->val = 0;
            } else if (type == BaseType::FLOAT) {
                literal->val = 0.0f;
            } else {
                throw unreachable_error{};
            }
            res[dim_idx_begin + i] = Ptr<Expr>{literal};
        }
        return dim_idx_begin + dim_len;
    }
    if (holds_alternative<Ptr<Expr>>(init.val)) {
        // this is a value, not a init list
        // TODO: evaluate the value for const vardef
        auto &expr = get<Ptr<Expr>>(init.val);
        res[dim_idx_begin] = Ptr<Expr>{};
        res[dim_idx_begin]->swap(expr);
        return off + 1;
    }
    // this is a init list
    auto &list = get<PtrList<RawVarDefStmt::InitList>>(init.val);
    for (size_t i = 0; i < list.size(); i++) {
        off = _pack_initval(*list[i], depth + 1, off, dims, res, type);
    }
    // if there're undefined initval, off should be set to dim_idx_begin +
    // dim_len
    return max(off, dim_idx_begin + dim_len);
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
                          vardef->init_vals, vardef->type);
        }
        if (vardef->is_const) {
            _const_table.insert(vardef->var_name, vardef);
        }
        ret.push_back(Ptr<VarDefStmt>{vardef});
    }
    return ret;
}

any ASTBuilder::visit(const Root &node) {
    auto &n = const_cast<Root &>(node);
    for (auto it = n.globals.begin(); it != n.globals.end();) {
        if (is<RawFunDefGlobal>(it->get())) {
            auto &raw = dynamic_cast<RawFunDefGlobal &>(*(it->get()));
            auto fundef = new FunDefGlobal;
            fundef->body.swap(raw.body);
            fundef->fun_name = raw.fun_name;
            fundef->ret_type = raw.ret_type;
            // parse params
            for (auto &raw_param : raw.params) {
                FunDefGlobal::Param param;
                param.type = raw_param->type;
                param.name = raw_param->name;
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
            // iterator after the insert point is invalidated
            it = n.globals.emplace(it, fundef);
            it += 1;
            // check for more vardef stmt
            visit(*fundef->body);
        } else if (is<RawVarDefGlobal>(it->get())) {
            auto &raw_global = dynamic_cast<RawVarDefGlobal &>(**it);
            auto &raw_stmt = *raw_global.vardef_stmt;
            auto vardefs = _split_vardef(raw_stmt);
            it = n.globals.erase(it);
            for (auto &def : vardefs) {
                auto global = new VarDefGlobal;
                global->vardef_stmt.swap(def);
                it = n.globals.emplace(it, global);
                it += 1;
            }
        } else {
            throw unreachable_error{};
        }
    }
    return {};
}

any ASTBuilder::visit(const BlockStmt &node) {
    // adjust scope
    _const_table.push();
    auto &n = const_cast<BlockStmt &>(node);
    for (auto it = n.stmts.begin(); it != n.stmts.end();) {
        if (not is<RawVarDefStmt>(it->get())) {
            // check for while / if / block
            visit(**it);
            ++it;
            continue;
        }
        auto &raw_stmt = dynamic_cast<RawVarDefStmt &>(**it);
        auto vardefs = _split_vardef(raw_stmt);
        it = n.stmts.erase(it);
        for (auto &def : vardefs) {
            auto new_stmt = static_cast<Stmt *>(def.release());
            it = n.stmts.insert(it, Ptr<Stmt>{new_stmt});
            it += 1;
        }
    }
    _const_table.pop();
    return {};
}

Ptr<Stmt> pack_vardefs_block(PtrList<VarDefStmt> &vardefs) {
    // we need a block stmt if vardefs.size() > 1
    auto block = new BlockStmt;
    for (auto &def : vardefs) {
        auto new_stmt = static_cast<Stmt *>(def.release());
        block->stmts.push_back(Ptr<Stmt>{new_stmt});
    }
    auto new_stmt = Ptr<Stmt>(block);
    return new_stmt;
}

any ASTBuilder::visit(const WhileStmt &node) {
    auto &n = const_cast<WhileStmt &>(node);
    if (not is<RawVarDefStmt>(n.body.get())) {
        // check for while / if / block
        visit(*n.body);
        return {};
    }
    auto &raw_stmt = dynamic_cast<RawVarDefStmt &>(*n.body);
    auto vardefs = _split_vardef(raw_stmt);
    // we need a block stmt if vardefs.size() > 1
    auto block = pack_vardefs_block(vardefs);
    n.body.swap(block);
    return {};
}

any ASTBuilder::visit(const IfStmt &node) {
    auto &n = const_cast<IfStmt &>(node);
    if (is<RawVarDefStmt>(n.then_body.get())) {
        auto &raw_then = dynamic_cast<RawVarDefStmt &>(*n.then_body);
        auto vardefs = _split_vardef(raw_then);
        // we need a block stmt if vardefs.size() > 1
        auto block = pack_vardefs_block(vardefs);
        n.then_body.swap(block);
    } else {
        // check for more vardef / while / if stmt
        visit(*n.then_body);
    }
    if (n.else_body.has_value() &&
        is<RawVarDefStmt>(n.else_body.value().get())) {
        auto &raw_else = dynamic_cast<RawVarDefStmt &>(*n.else_body.value());
        auto vardefs = _split_vardef(raw_else);
        // we need a block stmt if vardefs.size() > 1
        auto block = pack_vardefs_block(vardefs);
        n.else_body.value().swap(block);
    } else if (n.else_body.has_value()) {
        // check for while / if / block
        visit(*n.else_body.value());
    }
    return {};
}

AST::AST(RawAST &&raw_ast) {
    root = raw_ast.release_root();
    ASTBuilder builder;
    builder.visit(*root);
}

ostream &ast::operator<<(ostream &os, const AST &ast) {
    ASTPrinter printer;
    os << any_cast<string>(ast.visit(printer));
    return os;
}
