#include "ir_builder.hh"
#include "ast.hh"
#include "basic_block.hh"
#include "constant.hh"
#include "err.hh"
#include "function.hh"
#include "global_variable.hh"
#include "instruction.hh"
#include "log.hh"
#include "module.hh"
#include "type.hh"
#include "utils.hh"
#include "value.hh"

#include <any>
#include <cstddef>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace ast;
using namespace ir;
using namespace std;

class Scope {
  public:
    void enter() { _stack.push_back({}); }

    void exit() { _stack.pop_back(); }

    void push(string name, ir::Value *val) {
        if (_stack.back().find(name) == _stack.back().end())
            _stack.back()[name] = val;
        else
            throw logic_error{"the name of " + name + " has been defined"};
    }

    ir::Value *find(const string &name) {
        for (int i = _stack.size() - 1; i >= 0; i--) {
            if (_stack[i].find(name) != _stack[i].end())
                return _stack[i][name];
        }
        throw logic_error{name + " hasn't been defined"};
    }

    bool is_in_global() { return _stack.size() == 1; }

  private:
    vector<map<string, ir::Value *>> _stack;
};

class IRBuilderImpl : public ast::ASTVisitor {
  public:
    IRBuilderImpl() {
        _m = unique_ptr<ir::Module>(new ir::Module("Sysy Module"));
        load_external_symbol();
    }

    unique_ptr<ir::Module> release_module() { return std::move(_m); }

  private:
    Scope scope;
    unique_ptr<ir::Module> _m;

    /* extern symbol table*/
    void load_external_symbol() {
        scope.enter();
        // add extern symbol while running
        // this is only used in testing
        auto int_type = Types::get().int_type();
        auto float_type = Types::get().float_type();
        auto void_type = Types::get().void_type();
        bool external = true;
        scope.push("putint", _m->create_func(
                                 Types::get().func_type(void_type, {int_type}),
                                 "putint", external));
        scope.push("getint",
                   _m->create_func(Types::get().func_type(int_type, {}),
                                   "getint", external));
        scope.push("putch", _m->create_func(
                                Types::get().func_type(void_type, {int_type}),
                                "putch", external));
        scope.push("getch",
                   _m->create_func(Types::get().func_type(int_type, {}),
                                   "getch", external));
        scope.push(
            "getarray",
            _m->create_func(Types::get().func_type(
                                int_type, {Types::get().ptr_type(int_type)}),
                            "getarray", external));
        scope.push(
            "putarray",
            _m->create_func(
                Types::get().func_type(
                    void_type, {int_type, Types::get().ptr_type(int_type)}),
                "putarray", external));
        scope.push("putfloat", _m->create_func(Types::get().func_type(
                                                   void_type, {float_type}),
                                               "putfloat", external));
        scope.push("getfloat",
                   _m->create_func(Types::get().func_type(float_type, {}),
                                   "getfloat", external));
        scope.push(
            "getfarray",
            _m->create_func(Types::get().func_type(
                                int_type, {Types::get().ptr_type(float_type)}),
                            "getfarray", external));
        scope.push(
            "putfarray",
            _m->create_func(
                Types::get().func_type(
                    void_type, {int_type, Types::get().ptr_type(float_type)}),
                "putfarray", external));
        scope.push(
            "_sysy_starttime",
            _m->create_func(Types::get().func_type(void_type, {int_type}),
                            "_sysy_starttime", external));
        scope.push(
            "_sysy_stoptime",
            _m->create_func(Types::get().func_type(void_type, {int_type}),
                            "_sysy_stoptime", external));
    }

    /* variables that tansform information */
    Function *cur_func;
    BasicBlock *cur_bb;
    Value *ret_addr;

    vector<BasicBlock *> true_bb;  // and or while if use
    vector<BasicBlock *> false_bb; // and or while if use
    vector<BasicBlock *> next_bb;  // while if use
    unsigned loop_idx = 0;         // only while use

    set<Value *> const_table;

    Types &types = Types::get(); // Convenient reference to Types object for
                                 // easy access to members.
    Constants &constants =
        Constants::get(); // Convenient reference to Constants object for easy
                          // access to members.

    enum class type_of {
        Func,
        Param,
        Variant
    }; // label of caller that needs the conversion from ast type to ir type

    /* functions that are reused multiple times */
    inline Constant *zero_init(BaseType type) { // BaseType zero initilization
        if (type == BaseType::INT)
            return constants.int_const(0);
        else if (type == BaseType::FLOAT)
            return constants.float_const(0);
        else
            throw logic_error{"Void Type can't be zero-initialized"};
    }

    Type *ast2ir_ty(BaseType ty, type_of target,
                    const vector<size_t> &dims =
                        {}) { // return ir type according to ast type
        Type *base_type;
        if (ty == BaseType::INT)
            base_type = types.int_type();
        else if (ty == BaseType::FLOAT)
            base_type = types.float_type();
        else if (ty == BaseType::VOID && target == type_of::Func)
            base_type = types.void_type();
        else if (ty == BaseType::VOID)
            throw logic_error{"only function can use void type"};
        else
            throw logic_error{
                "the type to be converted doesn't belong to BaseType"};
        Type *elem_type = base_type;
        for (int i = dims.size() - 1; i > 0; i--)
            elem_type = types.array_type(elem_type, dims[i]);
        if (target == type_of::Param and not dims.empty())
            return types.ptr_type(elem_type);
        else if (not dims.empty())
            return types.array_type(elem_type, dims[0]);
        else
            return elem_type;
    }

    void
    type_convert(Value *&val,
                 Type *target) { // convert the type of the var to target type
        if (target->is<IntType>()) {
            if (val->get_type()->is<BoolType>())
                val = cur_bb->create_inst<ZextInst>(val);
            if (val->get_type()->is<FloatType>())
                val = cur_bb->create_inst<Fp2siInst>(val);
        } else if (target->is<FloatType>()) {
            if (val->get_type()->is<BoolType>())
                val = cur_bb->create_inst<ZextInst>(val);
            if (val->get_type()->is<IntType>())
                val = cur_bb->create_inst<Si2fpInst>(val);
        } else if (target->is<BoolType>()) {
            if (val->get_type()->is<IntType>())
                val = cur_bb->create_inst<ICmpInst>(ICmpInst::NE, val,
                                                    constants.int_const(0));
            if (val->get_type()->is<FloatType>())
                val = cur_bb->create_inst<FCmpInst>(FCmpInst::FNE, val,
                                                    constants.float_const(0));
        } else if (target->is<VoidType>()) {
            throw logic_error{"val can't be converted to void"};
        } else if (!val->get_type()->is<PointerType>()) {
            throw logic_error{"val can't be converted to array"};
        } else {
            return;
        }
    }

    void add_brinst(Value *oper) { // create br instruction in if/while/or/and
        if (oper->get_type()->is<IntType>())
            oper = cur_bb->create_inst<ICmpInst>(ICmpInst::NE, oper,
                                                 zero_init(BaseType::INT));
        if (oper->get_type()->is<FloatType>())
            oper = cur_bb->create_inst<FCmpInst>(FCmpInst::FNE, oper,
                                                 zero_init(BaseType::FLOAT));
        cur_bb->create_inst<BrInst>(oper->as<Instruction>(), true_bb.back(),
                                    false_bb.back());
    }

    vector<Value *> unflatten_idx(const vector<size_t> &dims,
                                  int idx) { // get unflatten indexes of array
        vector<Value *> idxs;
        for (int i = dims.size() - 1; i >= 0; i--) {
            idxs.insert(idxs.begin(), constants.int_const(idx % dims[i]));
            idx /= dims[i];
        }
        idxs.insert(idxs.begin(), constants.int_const(0));
        return idxs;
    }

    Value *get_addr(Value *base_addr,
                    const PtrList<Expr> &idxs) { // calculate the indexes of
                                                 // the variable in GEP
        auto elem_type =
            base_addr->get_type()->as<PointerType>()->get_elem_type();
        if ((base_addr->is<Argument>() || elem_type->is_basic_type()) &&
            idxs.empty()) {
            return base_addr;
        }

        vector<Value *> index;
        if (!is_a<Argument>(base_addr)) { // add 0 in front of the index
            index.push_back(zero_init(BaseType::INT));
        }
        for (auto &idxs : idxs) {
            index.push_back(any_cast<Value *>(visit(*idxs)));
        }

        size_t dims = elem_type->is_basic_type()
                          ? 0
                          : elem_type->as<ArrayType>()->get_dims();

        if (dims > index.size() - 1) // add 0 in the back of the index
            index.push_back(zero_init(BaseType::INT));

        auto addr =
            cur_bb->create_inst<GetElementPtrInst>(base_addr, std::move(index));
        return addr;
    }

    void short_circuit(vector<BasicBlock *> &short_circuit_bb,
                       vector<BasicBlock *> &next_bb,
                       const BinaryExpr &node) { // create a short-circuit
                                                 // evaluation jump block
        // Step1 push next bb
        next_bb.push_back(cur_func->create_bb());
        auto lhs = visit(*node.lhs);
        if (lhs.has_value())
            add_brinst(any_cast<Value *>(lhs));
        cur_bb = next_bb.back();
        // Step2 pop next bb
        next_bb.pop_back();
        auto rhs = visit(*node.rhs);
        if (rhs.has_value())
            add_brinst(any_cast<Value *>(rhs));
        // Step3 set insert point to short-circuit bb
        cur_bb = short_circuit_bb.back();
    }

    Value *bin_expr(Value *lhs, Value *rhs, BinOp op) {
        map<BinOp, IBinaryInst::IBinOp> iop_bin{
            {BinOp::ADD, IBinaryInst::ADD},
            {BinOp::SUB, IBinaryInst::SUB},
            {BinOp::MUL, IBinaryInst::MUL},
            {BinOp::DIV, IBinaryInst::SDIV},
            {BinOp::MOD, IBinaryInst::SREM}};
        map<BinOp, FBinaryInst::FBinOp> fop_bin{
            {BinOp::ADD, FBinaryInst::FADD},
            {BinOp::SUB, FBinaryInst::FSUB},
            {BinOp::MUL, FBinaryInst::FMUL},
            {BinOp::DIV, FBinaryInst::FDIV}};
        if (lhs->get_type()->is<FloatType>() or
            rhs->get_type()->is<FloatType>()) {
            type_convert(lhs, types.float_type());
            type_convert(rhs, types.float_type());
            return cur_bb->create_inst<FBinaryInst>(fop_bin[op], lhs, rhs);
        } else {
            type_convert(lhs, types.int_type());
            type_convert(rhs, types.int_type());
            return cur_bb->create_inst<IBinaryInst>(iop_bin[op], lhs, rhs);
        }
    }

    Value *cmp_expr(Value *lhs, Value *rhs, BinOp op) {
        map<BinOp, ICmpInst::ICmpOp> iop_cmp{
            {BinOp::LT, ICmpInst::LT}, {BinOp::GT, ICmpInst::GT},
            {BinOp::LE, ICmpInst::LE}, {BinOp::GE, ICmpInst::GE},
            {BinOp::EQ, ICmpInst::EQ}, {BinOp::NE, ICmpInst::NE}};
        map<BinOp, FCmpInst::FCmpOp> fop_cmp{
            {BinOp::LT, FCmpInst::FLT}, {BinOp::GT, FCmpInst::FGT},
            {BinOp::LE, FCmpInst::FLE}, {BinOp::GE, FCmpInst::FGE},
            {BinOp::EQ, FCmpInst::FEQ}, {BinOp::NE, FCmpInst::FNE}};
        if (lhs->get_type()->is<FloatType>() or
            rhs->get_type()->is<FloatType>()) {
            type_convert(lhs, types.float_type());
            type_convert(rhs, types.float_type());
            return cur_bb->create_inst<FCmpInst>(fop_cmp[op], lhs, rhs);
        } else {
            type_convert(lhs, types.int_type());
            type_convert(rhs, types.int_type());
            return cur_bb->create_inst<ICmpInst>(iop_cmp[op], lhs, rhs);
        }
    }

    // combine low-dimensional constant into high-dimensional constarray or
    // constzero
    // this method may overflow the stack in extreme cases(int
    // a[1][1][1]......) and have low performance in sparse initial value array.
    Constant *const_initializer(Type *type, size_t off,
                                map<size_t, Constant *>::const_iterator &iter,
                                map<size_t, Constant *>::const_iterator &end) {
        auto left_is_all_zero = iter == end;
        if (left_is_all_zero) { // no more init value left, all should be zero
            return constants.get().zero_const(type);
        }

        // leaf node
        if (type->is_basic_type()) {
            assert(off <= iter->first);
            if (off == iter->first) {
                auto con = iter->second;
                ++iter;
                return con;
            } else {
                return constants.get().zero_const(type);
            }
        }

        auto arr_type = type->as<ArrayType>();
        bool all_zero = true;
        size_t step = arr_type->get_total_cnt() / arr_type->get_elem_cnt();
        vector<Constant *> sub_consts;
        for (size_t i = 0; i < arr_type->get_elem_cnt(); ++i) {
            auto sub_const =
                const_initializer(arr_type->get_elem_type(), off, iter, end);
            sub_consts.push_back(sub_const);
            off += step;
            all_zero &= sub_const->is<ConstZero>();
        }
        if (all_zero) {
            return constants.get().zero_const(type);
        } else {
            return constants.get().array_const(std::move(sub_consts));
        }
    }

    // only for conversion of float to int or int to float
    // because the type of literalexpr doesn't match the base type of array
    Constant *const_type_convert(Constant *con, Type *target) {
        if (con->get_type()->is_basic_type()) {
            if (con->get_type() != target) {
                if (con->is<ConstFloat>()) {
                    return constants.int_const(con->as<ConstFloat>()->val());
                } else {
                    return constants.float_const(con->as<ConstInt>()->val());
                }
            } else {
                return con;
            }
        } else {
            return con;
        }
    }

    pair<map<size_t, Value *>, Constant *>
    get_initializer(const std::optional<std::map<size_t, Ptr<Expr>>> &init_vals,
                    Type *type) {
        map<size_t, Value *> init_var;
        map<size_t, Constant *> init_const;
        if (init_vals.has_value()) {
            for (auto &[off, init_val] : init_vals.value()) {
                auto init = any_cast<Value *>(visit(*init_val));
                if (init->is<Constant>()) {
                    // init_const[off] = init->as<Constant>();
                    init_const[off] = const_type_convert(
                        init->as<Constant>(),
                        type->is_basic_type()
                            ? type
                            : type->as<ArrayType>()->get_base_type());
                } else
                    init_var[off] = init;
            }
        }
        // const-initialization is requcired for unassigned
        auto begin_iter = init_const.cbegin();
        auto end_iter = init_const.cend();
        auto const_inits = const_initializer(type, 0, begin_iter, end_iter);
        return {init_var, const_inits};
    }

    /* visit for super class pointer */
    using ast::ASTVisitor::visit;
    /* do NOT save pointer to AST tree node in visitor */
    any visit(const ast::Root &node) override final;
    /* global */
    any visit(const ast::FunDefGlobal &node) override final;
    any visit(const ast::VarDefGlobal &node) override final;
    /* stmt */
    any visit(const ast::BlockStmt &node) override final;
    any visit(const ast::IfStmt &node) override final;
    any visit(const ast::WhileStmt &node) override final;
    any visit(const ast::BreakStmt &node) override final;
    any visit(const ast::ContinueStmt &node) override final;
    any visit(const ast::ReturnStmt &node) override final;
    any visit(const ast::AssignStmt &node) override final;
    any visit(const ast::VarDefStmt &node) override final;
    any visit(const ast::ExprStmt &node) override final;
    /* expr */
    any visit(const ast::CallExpr &node) override final;
    any visit(const ast::LiteralExpr &node) override final;
    any visit(const ast::LValExpr &node) override final;
    any visit(const ast::BinaryExpr &node) override final;
    any visit(const ast::UnaryExpr &node) override final;
};

IRBuilder::IRBuilder(const ast::AST &ast) {
    auto builder = IRBuilderImpl{};
    ast.accept(builder);
    _module = builder.release_module();
}

any IRBuilderImpl::visit(const Root &node) {
    scope.enter();
    for (auto &gv_ptr : node.globals)
        visit(*gv_ptr);
    _m->set_main(as_a<Function>(scope.find("main")));
    scope.exit();
    return {};
}

/* global */
any IRBuilderImpl::visit(const FunDefGlobal &node) {
    // create function
    Type *ret_type = ast2ir_ty(node.ret_type, type_of::Func);
    vector<Type *> param_types;
    for (const auto &param : node.params) {
        param_types.push_back(
            ast2ir_ty(param.type, type_of::Param, param.dims));
    }
    FuncType *func_type = types.func_type(ret_type, std::move(param_types));
    auto name = node.fun_name;
    cur_func = _m->create_func(func_type, std::move(name));
    scope.push(name, cur_func);

    // initialize params;
    scope.enter();
    cur_func->create_bb(); // create entry_bb
    cur_func->create_bb(); // create exit_bb
    auto entry_to = cur_bb = cur_func->create_bb();
    for (unsigned i = 0; i < node.params.size(); i++)
        if (node.params[i]
                .dims.empty()) { // If the parameter is a basic variable, it is
                                 // stored in a temporary address to synchronize
                                 // with AssignStmt
                                 // (only parameter may not have address)
            auto addr = cur_func->get_entry_bb()->create_inst<AllocaInst>(
                param_types[i]);
            cur_bb->create_inst<StoreInst>(cur_func->get_args()[i], addr);
            scope.push(node.params[i].name, addr);
        } else {
            scope.push(node.params[i].name, cur_func->get_args()[i]);
        }

    if (node.ret_type != BaseType::VOID) { // create return value
        ret_addr = cur_func->get_entry_bb()->create_inst<AllocaInst>(ret_type);

        if (cur_func->get_name() == "@main") { // for the function except main,
                                               // it returns undef value
            cur_bb->create_inst<StoreInst>(zero_init(node.ret_type), ret_addr);
        }
        auto ret_val = cur_func->get_exit_bb()->create_inst<LoadInst>(ret_addr);
        cur_func->get_exit_bb()->create_inst<RetInst>(ret_val);
    } else {
        cur_func->get_exit_bb()->create_inst<RetInst>();
    }

    visit(*node.body);
    if (not cur_bb->is_terminated())
        cur_bb->create_inst<BrInst>(cur_func->get_exit_bb());
    cur_func->get_entry_bb()->create_inst<BrInst>(entry_to);
    scope.exit();
    return {};
}

std::any IRBuilderImpl::visit(const VarDefGlobal &node) {
    const auto &suc_node = *node.vardef_stmt;
    if (suc_node.is_const && suc_node.dims.empty()) { // define int or float
        Value *init;
        if (suc_node.init_vals.has_value() && !suc_node.init_vals->empty()) {
            auto &value = *suc_node.init_vals.value().find(0)->second;
            init = any_cast<Value *>(visit(value));
        } else
            init = zero_init(suc_node.type);
        scope.push(suc_node.var_name, init);
        const_table.insert(init);
    } else { // define array
        auto type = ast2ir_ty(suc_node.type, type_of::Variant, suc_node.dims);
        // calculate the initial values
        auto [init_vars, const_inits] =
            get_initializer(suc_node.init_vals, type);

        if (!init_vars.empty())
            throw logic_error{"Initialization values for global variables "
                              "should have compile-time constants"};

        // define variable
        auto name = suc_node.var_name;
        auto var = _m->create_global_var(type, std::move(name), const_inits);
        scope.push(suc_node.var_name, var);
        if (suc_node.is_const) {
            const_table.insert(var);
        }
    }
    return {};
}

/* stmt */
any IRBuilderImpl::visit(const BlockStmt &node) {
    scope.enter();
    for (auto &stmt : node.stmts) {
        visit(*stmt);
        // for break / continue / return
        if (cur_bb->is_terminated()) {
            break;
        }
    }
    scope.exit();
    return {};
}

any IRBuilderImpl::visit(const IfStmt &node) {
    bool else_exist = node.else_body.has_value();
    // Step1 push true bb and false bb into stack and create next bb
    true_bb.push_back(cur_func->create_bb());  // true_bb
    false_bb.push_back(cur_func->create_bb()); // false_bb;
    next_bb.push_back(else_exist ? cur_func->create_bb() : false_bb.back());
    auto cond = visit(*node.cond);
    if (cond.has_value())
        add_brinst(any_cast<Value *>(cond));
    scope.enter(); // then_body has its own scope regardless of whether it
                   // is expr or block
    cur_bb = true_bb.back();
    visit(*node.then_body);
    // Step2 backfill next bb to true_bb
    if (not cur_bb->is_terminated())
        cur_bb->create_inst<BrInst>(next_bb.back());
    scope.exit();
    if (else_exist) {
        scope.enter();
        cur_bb = false_bb.back();
        visit(*node.else_body.value());
        // Step2 backfill next bb to false_bb
        if (not cur_bb->is_terminated())
            cur_bb->create_inst<BrInst>(next_bb.back());
        scope.exit();
    }
    // Step3 set the insert point to next_bb
    cur_bb = next_bb.back();
    // Step4 pop out bbs created by if
    true_bb.pop_back();
    false_bb.pop_back();
    next_bb.pop_back();
    return {};
}

any IRBuilderImpl::visit(const WhileStmt &node) {
    // Step1 Check whether cur_bb is empty
    BasicBlock *cond_bb = cur_bb;
    if (cur_bb->insts().size() != 0 || cur_bb == cur_func->get_entry_bb()) {
        cond_bb = cur_func->create_bb();
        cur_bb->create_inst<BrInst>(cond_bb); // cond_bb
        cur_bb = cond_bb;
    }
    // Step2 push true_bb, false_bb and next_bb into stack;
    true_bb.push_back(cur_func->create_bb());  // body_bb
    false_bb.push_back(cur_func->create_bb()); // next_bb
    next_bb.push_back(cond_bb);                // cond_bb
    unsigned last_loop_idx = loop_idx;
    loop_idx = next_bb.size(); // add loop index
    auto cond = visit(*node.cond);
    if (cond.has_value())
        add_brinst(any_cast<Value *>(cond));
    cur_bb = true_bb.back();
    scope.enter();
    visit(*node.body);
    // Step3 backfill cond_bb to body_bb
    if (not cur_bb->is_terminated())
        cur_bb->create_inst<BrInst>(cond_bb);
    scope.exit();
    // Step4 set insert_point to false_bb
    cur_bb = false_bb.back();
    // Step5 pop out the bbs created by while when exiting loop
    true_bb.pop_back();
    false_bb.pop_back();
    next_bb.pop_back();
    loop_idx = last_loop_idx;
    return {};
}

any IRBuilderImpl::visit(const BreakStmt &node) {
    if (loop_idx == 0)
        throw logic_error{"break must be used in a loop"};
    cur_bb->create_inst<BrInst>(false_bb[loop_idx - 1]);
    return {};
}

any IRBuilderImpl::visit(const ContinueStmt &node) {
    if (loop_idx == 0)
        throw logic_error{"continue must be used in a loop"};
    cur_bb->create_inst<BrInst>(next_bb[loop_idx - 1]);
    return {};
}

any IRBuilderImpl::visit(const ReturnStmt &node) {
    if (node.ret_val.has_value()) {
        auto ret_val = any_cast<Value *>(visit(*node.ret_val.value()));
        Type *ret_type =
            cur_func->get_type()->as<FuncType>()->get_result_type();
        type_convert(ret_val, ret_type);
        cur_bb->create_inst<StoreInst>(ret_val, ret_addr);
    }
    cur_bb->create_inst<BrInst>(cur_func->get_exit_bb());
    return {};
}

any IRBuilderImpl::visit(const AssignStmt &node) {
    Value *val = any_cast<Value *>(visit(*node.val));
    Value *addr = scope.find(node.var_name);
    if (const_table.find(addr) != const_table.end())
        throw logic_error{"const variable can't be assigned a value"};
    addr = get_addr(addr, node.idxs);
    type_convert(val, addr->get_type()->as<PointerType>()->get_elem_type());
    cur_bb->create_inst<StoreInst>(val, addr);
    return {};
}

any IRBuilderImpl::visit(const VarDefStmt &node) {
    if (node.is_const && node.dims.empty()) { // define const int or float
        Value *init;
        if (node.init_vals.has_value() && !node.init_vals->empty()) {
            auto &value = *node.init_vals.value().find(0)->second;
            init = any_cast<Value *>(visit(value));
        } else
            init = zero_init(node.type);
        scope.push(node.var_name, init);
        const_table.insert(init);
    } else {
        auto type = ast2ir_ty(node.type, type_of::Variant, node.dims);
        auto [init_vars, const_inits] = get_initializer(node.init_vals, type);

        // define variable
        auto begin = cur_func->get_entry_bb()->insts().begin();
        auto var =
            cur_func->get_entry_bb()->insert_inst<AllocaInst>(begin, type);
        scope.push(node.var_name, var);

        if (node.init_vals.has_value()) {
            cur_bb->create_inst<StoreInst>(const_inits, var);
        }
        if (node.is_const) {
            const_table.insert(var);
        }
        // assign initial value to variable
        for (auto [off, val] : init_vars) {
            Value *addr;
            if (node.dims.empty()) {
                addr = var;
            } else {
                auto idxs = unflatten_idx(node.dims, off);
                addr = cur_bb->create_inst<GetElementPtrInst>(var,
                                                              std::move(idxs));
            }
            type_convert(val,
                         addr->get_type()->as<PointerType>()->get_elem_type());
            cur_bb->create_inst<StoreInst>(val, addr);
        }
    }
    return {};
}

any IRBuilderImpl::visit(const ExprStmt &node) {
    if (node.expr.has_value())
        visit(*node.expr.value());
    return {};
}

/* expr */
any IRBuilderImpl::visit(const CallExpr &node) {
    Value *func = scope.find(node.fun_name);
    if (!func->is<Function>())
        throw logic_error{node.fun_name + " is not a function name"};
    vector<Value *> args;
    auto func_type = func->as<Function>()->get_type()->as<FuncType>();
    for (auto &arg : node.args) {
        auto param = any_cast<Value *>(visit(*arg));
        type_convert(param, func_type->get_param_type(args.size()));
        args.push_back(param);
    }
    return cur_bb->create_inst<CallInst>(func->as<Function>(), std::move(args))
        ->as<Value>();
}

any IRBuilderImpl::visit(const LiteralExpr &node) {
    if (node.type == BaseType::INT)
        return dynamic_cast<Value *>(constants.int_const(get<int>(node.val)));
    else if (node.type == BaseType::FLOAT)
        return dynamic_cast<Value *>(
            constants.float_const(get<float>(node.val)));
    else
        throw logic_error{"the type of LiteralExpr is void"};
}

any IRBuilderImpl::visit(const LValExpr &node) { // return the value of the var
    Value *var = scope.find(node.var_name);
    if (!var->get_type()->is<PointerType>()) {
        return var;
    } // const val store it's init value into scope

    auto addr = get_addr(var, node.idxs);

    // calculate the num of index of the var when load value
    size_t num_idx{0};
    if (var->is<Argument>()) {
        num_idx = 1;
    }
    auto elem_type = var->get_type()->as<PointerType>()->get_elem_type();
    if (elem_type->is<ArrayType>())
        num_idx += elem_type->as<ArrayType>()->get_dims();

    if (node.idxs.size() == num_idx) {
        var = cur_bb->create_inst<LoadInst>(addr);
    } else {
        var = addr;
    }
    return var;
}

any IRBuilderImpl::visit(const BinaryExpr &node) {
    Value *lhs, *rhs;
    switch (node.op) {
    case ast::BinOp::ADD:
    case ast::BinOp::SUB:
    case ast::BinOp::MUL:
    case ast::BinOp::DIV:
    case ast::BinOp::MOD:
        lhs = any_cast<Value *>(visit(*node.lhs));
        rhs = any_cast<Value *>(visit(*node.rhs));
        return bin_expr(lhs, rhs, node.op);
    case ast::BinOp::LT:
    case ast::BinOp::GT:
    case ast::BinOp::LE:
    case ast::BinOp::GE:
    case ast::BinOp::EQ:
    case ast::BinOp::NE:
        rhs = any_cast<Value *>(visit(*node.rhs));
        lhs = any_cast<Value *>(visit(*node.lhs));
        return cmp_expr(lhs, rhs, node.op);
    case ast::BinOp::AND:
        short_circuit(false_bb, true_bb, node);
        return {};
    case ast::BinOp::OR:
        short_circuit(true_bb, false_bb, node);
        return {};
    }
    throw unreachable_error{};
}

any IRBuilderImpl::visit(const UnaryExpr &node) {
    Value *val = nullptr;
    switch (node.op) {
    case ast::UnaryOp::PlUS:
        val = any_cast<Value *>(visit(*node.rhs));
        break;
    case ast::UnaryOp::MINUS:
        val = any_cast<Value *>(visit(*node.rhs));
        if (val->get_type()->is<BoolType>())
            val = cur_bb->create_inst<ZextInst>(val);
        if (val->get_type()->is<IntType>())
            val = cur_bb->create_inst<IBinaryInst>(
                IBinaryInst::SUB, zero_init(BaseType::INT), val);
        if (val->get_type()->is<FloatType>())
            val = cur_bb->create_inst<FBinaryInst>(
                FBinaryInst::FSUB, zero_init(BaseType::FLOAT), val);
        break;
    case ast::UnaryOp::NOT:
        val = any_cast<Value *>(visit(*node.rhs));
        if (val->get_type()->is<BoolType>())
            val = cur_bb->create_inst<IBinaryInst>(IBinaryInst::XOR, val,
                                                   constants.bool_const(true));
        if (val->get_type()->is<IntType>())
            val = cur_bb->create_inst<ICmpInst>(ICmpInst::EQ, val,
                                                zero_init(BaseType::INT));
        if (val->get_type()->is<FloatType>())
            val = cur_bb->create_inst<FCmpInst>(FCmpInst::FEQ, val,
                                                zero_init(BaseType::FLOAT));
        break;
    }
    return val;
}
