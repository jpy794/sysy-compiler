#include "sysy_builder.hh"
#include "ast.hh"
#include "basic_block.hh"
#include "constant.hh"
#include "err.h"
#include "function.hh"
#include "instruction.hh"
#include "type.hh"
#include "utils.hh"
#include "value.hh"

#include <any>
#include <stdexcept>
#include <utility>
#include <vector>
using namespace ast;
using namespace ir;
using std::vector;
Types &types = Types::get();
Constants &constants = Constants::get();
inline Constant *ZeroInit(BaseType type) {
    if (type == BaseType::INT)
        return constants.int_const(0);
    else if (type == BaseType::FLOAT)
        return constants.float_const(0);
    else
        throw std::logic_error{"Void Type can't be zero-initialized"};
}

Function *cur_func;
BasicBlock *cur_bb;

std::vector<BasicBlock *> true_bb;  // and or while if use
std::vector<BasicBlock *> false_bb; // and or while if use
std::vector<BasicBlock *> next_bb;  // only while use

enum class TypeOf { Func, Param, Variant };
Type *ConvertType(BaseType ty, TypeOf callee, const vector<size_t> &dims = {}) {
    Type *base_type;
    if (ty == BaseType::INT)
        base_type = types.int_type();
    else if (ty == BaseType::FLOAT)
        base_type = types.float_type();
    else if (ty == BaseType::VOID && callee == TypeOf::Func)
        base_type = types.void_type();
    else if (ty == BaseType::VOID)
        throw std::logic_error{"only function can use void type"};
    else
        throw std::logic_error{
            "the type to be converted doesn't belong to BaseType"};
    Type *elem_type = base_type;
    for (int i = dims.size() - 1; i > 0; i--)
        elem_type = types.array_type(elem_type, dims[i]);
    if (callee == TypeOf::Param and not dims.empty())
        return types.ptr_type(elem_type);
    else if (not dims.empty())
        return types.array_type(elem_type, dims[0]);
    else
        return elem_type;
}

void AddBrInst(Value *oper) {
    if (oper != nullptr) {
        if (oper->get_type()->is<IntType>())
            oper = cur_bb->create_inst<CmpInst>(CmpInst::NE, oper,
                                                ZeroInit(BaseType::INT));
        if (oper->get_type()->is<FloatType>())
            oper = cur_bb->create_inst<CmpInst>(CmpInst::FNE, oper,
                                                ZeroInit(BaseType::FLOAT));
        cur_bb->create_inst<BrInst>(oper, true_bb.back(), false_bb.back());
    }
}

std::any SysyBuilder::visit(const Root &node) {
    scope.enter();
    for (auto &gv_ptr : node.globals)
        visit(*gv_ptr);
    scope.exit();
    return {};
}
/* global */
std::any SysyBuilder::visit(const FunDefGlobal &node) {
    scope.enter();
    Type *ret_type = ConvertType(node.ret_type, TypeOf::Func);
    vector<Type *> param_types;
    for (const auto &param : node.params) {
        param_types.push_back(
            ConvertType(param.type, TypeOf::Param, param.dims));
    }
    FuncType *func_type = types.func_type(ret_type, std::move(param_types));
    auto name = node.fun_name;
    cur_func = _m->create_func(func_type, std::move(name));
    for (unsigned i = 0; i < node.params.size(); i++)
        scope.push(node.params[i].name, cur_func->get_args()[i]);
    cur_bb = cur_func->create_bb(); // create entry_bb
    visit(*node.body);
    if (not cur_bb->is_terminated())
        switch (node.ret_type) { // FIXME:the branch without return stmt returns
                                 // undef value
        case ast::BaseType::INT:
        case ast::BaseType::FLOAT:
            cur_bb->create_inst<RetInst>(ZeroInit(node.ret_type));
            break;
        case ast::BaseType::VOID:
            cur_bb->create_inst<RetInst>();
            break;
        }
    scope.exit();
    return {};
}
std::any SysyBuilder::visit(const VarDefGlobal &node) {
    visit(*node.vardef_stmt);
    return {};
}
/* stmt */
std::any SysyBuilder::visit(const BlockStmt &node) {
    scope.enter();
    for (auto &stmt : node.stmts)
        visit(*stmt);
    scope.exit();
    return {};
}
std::any SysyBuilder::visit(const IfStmt &node) {
    bool else_exist = node.else_body.has_value();
    // Step1 push true bb and false bb into stack and create next bb
    true_bb.push_back(cur_func->create_bb());  // true_bb
    false_bb.push_back(cur_func->create_bb()); // false_bb;
    BasicBlock *next_bb = else_exist ? cur_func->create_bb() : false_bb.back();
    auto cond_val = std::any_cast<Value *>(visit(*node.cond));
    AddBrInst(cond_val);

    scope.enter(); // then_body has its own scope regardless of whether it is
                   // expr or block
    cur_bb = true_bb.back();
    visit(*node.then_body);
    // Step2 backfill next bb to true_bb
    if (not cur_bb->is_terminated())
        cur_bb->create_inst<BrInst>(next_bb);
    scope.exit();
    if (else_exist) {
        scope.enter();
        cur_bb = false_bb.back();
        visit(*node.else_body.value());
        // Step2 backfill next bb to false_bb
        if (not cur_bb->is_terminated())
            cur_bb->create_inst<BrInst>(next_bb);
        scope.exit();
    }
    // Step3 set the insert point to next_bb
    cur_bb = next_bb;
    // Step4 pop out bbs created by if
    true_bb.pop_back();
    false_bb.pop_back();
    return {};
}
std::any SysyBuilder::visit(const WhileStmt &node) {
    // Step1 Check whether cur_bb is empty
    BasicBlock *cond_bb = cur_bb;
    if (cur_bb->get_insts().size() != 0) {
        cond_bb = cur_func->create_bb();
        cur_bb->create_inst<BrInst>(cond_bb); // cond_bb
        cur_bb = cond_bb;
    }
    // Step2 push true_bb, false_bb and next_bb into stack;
    true_bb.push_back(cur_func->create_bb());  // body_bb
    false_bb.push_back(cur_func->create_bb()); // next_bb
    next_bb.push_back(cond_bb);                // cond_bb
    auto cond_val = std::any_cast<Value *>(visit(*node.cond));
    AddBrInst(cond_val);

    cur_bb = true_bb.back();
    scope.enter();
    visit(*node.body);
    // Step3 backfill cond_bb to body_bb
    if (not cur_bb->is_terminated())
        cur_bb->create_inst<BrInst>(cond_bb);
    scope.exit();
    // Step4 set insert_point to false_bb
    cur_bb = false_bb.back();
    // Step5 pop out the bbs created by while
    true_bb.pop_back();
    false_bb.pop_back();
    next_bb.pop_back();
    return {};
}
std::any SysyBuilder::visit(const BreakStmt &node) {
    if (next_bb.empty())
        throw std::logic_error{"break must be used in a loop"};
    cur_bb->create_inst<BrInst>(false_bb.back());
    return {};
}
std::any SysyBuilder::visit(const ContinueStmt &node) {
    if (next_bb.empty())
        throw std::logic_error{"continue must be used in a loop"};
    cur_bb->create_inst<BrInst>(next_bb.back());
    return {};
}
std::any SysyBuilder::visit(const ReturnStmt &node) {
    Value *ret_val = nullptr;
    if (node.ret_val.has_value())
        ret_val = std::any_cast<Value *>(visit(*node.ret_val.value()));
    if (ret_val)
        cur_bb->create_inst<RetInst>(ret_val);
    else
        cur_bb->create_inst<RetInst>();
    return {};
}
std::any SysyBuilder::visit(const AssignStmt &node) {
    Value *val = std::any_cast<Value *>(visit(*node.val));
    Value *var = scope.find(node.var_name);
    if (!var)
        throw std::logic_error{node.var_name + " isn't defined"};
    if (!node.idxs.empty()) {
        std::vector<Value *> index;
        if (!is_a<Argument>(var)) // add 0 in front of the index
            index.push_back(ZeroInit(BaseType::INT));
        for (auto &idxs : node.idxs)
            index.push_back(std::any_cast<Value *>(visit(*idxs)));
        auto elem_type = var->get_type()->as<PointerType>()->get_elem_type();
        if (!elem_type->is_basic_type() and
            elem_type->as<ArrayType>()->get_dims() >=
                index.size()) // add 0 in the back of the index
            index.push_back(ZeroInit(BaseType::INT));
        var = cur_bb->create_inst<GetElementPtrInst>(var, std::move(index));
    }
    cur_bb->create_inst<StoreInst>(val, var);
    return {};
}
Value *GetAddr();
std::any SysyBuilder::visit(const VarDefStmt &node) {
    if (node.is_const) {
        vector<Constant *> init_vals;
        for (auto &init_val : node.init_vals) {
            Constant *init;
            if (init_val.has_value())
                init = std::any_cast<Constant *>(visit(*init_val.value()));
            else {
                init = ZeroInit(node.type);
            }
            if (!init)
                throw std::logic_error{
                    "const init must be able to calculate in compiling"};
            init_vals.push_back(init);
        }
        if (node.dims.empty()) { // const type var = init
            scope.push(node.var_name, init_vals[0]);
        } else {
            auto inits = constants.array_const(std::move(init_vals));
            scope.push(node.var_name, inits);
        }
    } else {
        vector<Value *> init_vals;
        for (auto &init_val : node.init_vals) {
            if (!init_val.has_value())
                throw std::logic_error{"advise a zero initialization"};
            auto init = std::any_cast<Value *>(visit(*init_val.value()));
            if (!init)
                throw std::logic_error{"the var fails to initialize"};
            init_vals.push_back(init);
        }
        auto type = ConvertType(node.type, TypeOf::Variant, node.dims);
        auto var = cur_bb->create_inst<AllocaInst>(type);
        scope.push(node.var_name, var);
        // TODO:initilize
    }
    return {};
}
std::any SysyBuilder::visit(const ExprStmt &node) {
    if (node.expr.has_value())
        visit(*node.expr.value());
    return {};
}
/* expr */
std::any SysyBuilder::visit(const CallExpr &node) {
    Function *func = dynamic_cast<Function *>(scope.find(node.fun_name));
    if (func == nullptr)
        throw std::logic_error{node.fun_name + " is not a function name"};
    std::vector<Value *> args;
    for (auto &arg : node.args) {
        args.push_back(std::any_cast<Value *>(visit(*arg)));
    }
    return cur_bb->create_inst<CallInst>(func, std::move(args));
}
std::any SysyBuilder::visit(const LiteralExpr &node) {
    if (node.type == BaseType::INT)
        return dynamic_cast<Value *>(
            constants.int_const(std::get<int>(node.val)));
    else if (node.type == BaseType::FLOAT)
        return dynamic_cast<Value *>(
            constants.float_const(std::get<float>(node.val)));
    else
        throw std::logic_error{"the type of LiteralExpr is void"};
}
std::any SysyBuilder::visit(const LValExpr &node) {
    Value *var = scope.find(node.var_name);
    if (!var)
        throw std::logic_error{node.var_name + " isn't defined"};
    if (!node.idxs.empty()) { // TODO: refactor
        std::vector<Value *> index;
        if (!is_a<Argument>(var)) // add 0 in front of the index
            index.push_back(ZeroInit(BaseType::INT));
        for (auto &idxs : node.idxs)
            index.push_back(std::any_cast<Value *>(visit(*idxs)));
        auto elem_type = var->get_type()->as<PointerType>()->get_elem_type();
        if (!elem_type->is_basic_type() and
            elem_type->as<ArrayType>()->get_dims() >=
                index.size()) // add 0 in the back of the index
            index.push_back(ZeroInit(BaseType::INT));
        var = dynamic_cast<Value *>(
            cur_bb->create_inst<GetElementPtrInst>(var, std::move(index)));
    }
    var = cur_bb->create_inst<LoadInst>(var);
    return var;
}
std::any SysyBuilder::visit(const BinaryExpr &node) {
    // TODO: refactor
    if (node.op == ast::BinOp::OR) {
        // Step1 push false bb
        false_bb.push_back(cur_func->create_bb());
        auto lhs = std::any_cast<Value *>(visit(*node.lhs));
        AddBrInst(lhs);
        cur_bb = false_bb.back();
        // Step2 pop false bb
        false_bb.pop_back();
        auto rhs = std::any_cast<Value *>(visit(*node.rhs));
        AddBrInst(rhs);
        // Step3 set insert point to true bb
        cur_bb = true_bb.back();
        return {};
    }
    if (node.op == ast::BinOp::AND) {
        // Step1 push true bb
        true_bb.push_back(cur_func->create_bb());
        auto lhs = std::any_cast<Value *>(visit(*node.lhs));
        AddBrInst(lhs);
        cur_bb = false_bb.back();
        // Step2 pop true bb
        false_bb.pop_back();
        auto rhs = std::any_cast<Value *>(visit(*node.rhs));
        AddBrInst(rhs);
        // Step3 set insert point to true bb
        cur_bb = true_bb.back();
        return {};
    }
    auto lhs = std::any_cast<Value *>(visit(*node.lhs));
    auto rhs = std::any_cast<Value *>(visit(*node.rhs));
    bool to_float = false;
    if (lhs->get_type()->is<FloatType>() or
        rhs->get_type()->is<FloatType>()) // check whether it's neccessary to
                                          // convert int to float
        to_float = true;
    if (lhs->get_type()->is<BoolType>())
        lhs = cur_bb->create_inst<ZextInst>(lhs);
    if (lhs->get_type()->is<IntType>() && to_float)
        lhs = cur_bb->create_inst<Si2fpInst>(lhs);
    if (rhs->get_type()->is<BoolType>())
        rhs = cur_bb->create_inst<ZextInst>(rhs);
    if (rhs->get_type()->is<IntType>() && to_float)
        rhs = cur_bb->create_inst<Si2fpInst>(rhs);
    switch (node.op) {
    case ast::BinOp::ADD:
        if (to_float)
            return cur_bb->create_inst<BinaryInst>(BinaryInst::FADD, lhs, rhs);
        else
            return cur_bb->create_inst<BinaryInst>(BinaryInst::ADD, lhs, rhs);
        break;
    case ast::BinOp::SUB:
        if (to_float)
            return cur_bb->create_inst<BinaryInst>(BinaryInst::FSUB, lhs, rhs);
        else
            return cur_bb->create_inst<BinaryInst>(BinaryInst::SUB, lhs, rhs);
        break;
    case ast::BinOp::MUL:
        if (to_float)
            return cur_bb->create_inst<BinaryInst>(BinaryInst::FMUL, lhs, rhs);
        else
            return cur_bb->create_inst<BinaryInst>(BinaryInst::MUL, lhs, rhs);
        break;
    case ast::BinOp::DIV:
        if (to_float)
            return cur_bb->create_inst<BinaryInst>(BinaryInst::FDIV, lhs, rhs);
        else
            return cur_bb->create_inst<BinaryInst>(BinaryInst::SDIV, lhs, rhs);
        break;
    case ast::BinOp::MOD:
        if (to_float)
            return cur_bb->create_inst<BinaryInst>(BinaryInst::FREM, lhs, rhs);
        else
            return cur_bb->create_inst<BinaryInst>(BinaryInst::SREM, lhs, rhs);
        break;
    case ast::BinOp::LT:
        if (to_float)
            return cur_bb->create_inst<CmpInst>(CmpInst::FLT, lhs, rhs);
        else
            return cur_bb->create_inst<CmpInst>(CmpInst::LT, lhs, rhs);
        break;
    case ast::BinOp::GT:
        if (to_float)
            return cur_bb->create_inst<CmpInst>(CmpInst::FGT, lhs, rhs);
        else
            return cur_bb->create_inst<CmpInst>(CmpInst::GT, lhs, rhs);
        break;
    case ast::BinOp::LE:
        if (to_float)
            return cur_bb->create_inst<CmpInst>(CmpInst::FLE, lhs, rhs);
        else
            return cur_bb->create_inst<CmpInst>(CmpInst::LE, lhs, rhs);
        break;
    case ast::BinOp::GE:
        if (to_float)
            return cur_bb->create_inst<CmpInst>(CmpInst::FGE, lhs, rhs);
        else
            return cur_bb->create_inst<CmpInst>(CmpInst::GE, lhs, rhs);
        break;
    case ast::BinOp::EQ:
        if (to_float)
            return cur_bb->create_inst<CmpInst>(CmpInst::FEQ, lhs, rhs);
        else
            return cur_bb->create_inst<CmpInst>(CmpInst::EQ, lhs, rhs);
        break;
    case ast::BinOp::NE:
        if (to_float)
            return cur_bb->create_inst<CmpInst>(CmpInst::FNE, lhs, rhs);
        else
            return cur_bb->create_inst<CmpInst>(CmpInst::NE, lhs, rhs);
        break;
    case ast::BinOp::AND:
        break;
    case ast::BinOp::OR:
        break;
    }
    return {};
}
std::any SysyBuilder::visit(const UnaryExpr &node) {
    Value *val;
    switch (node.op) {
    case ast::UnaryOp::PlUS:
        // FIXME: what is plus? Can "+ var" be treated as "var"
        val = std::any_cast<Value *>(visit(*node.rhs));
        break;
    case ast::UnaryOp::MINUS:
        val = std::any_cast<Value *>(visit(*node.rhs));
        if (val->get_type()->is<BoolType>())
            throw std::logic_error{"the type of unaryop shouldn't be bool"};
        if (val->get_type()->is<IntType>())
            val = cur_bb->create_inst<BinaryInst>(BinaryInst::SUB,
                                                  ZeroInit(BaseType::INT), val);
        if (val->get_type()->is<FloatType>())
            val = cur_bb->create_inst<BinaryInst>(
                BinaryInst::FSUB, ZeroInit(BaseType::FLOAT), val);
        break;
    case ast::UnaryOp::NOT:
        val = std::any_cast<Value *>(visit(*node.rhs));
        if (val->get_type()->is<BoolType>())
            throw std::logic_error{"the type of unaryop shouldn't be bool"};
        if (val->get_type()->is<IntType>())
            val = cur_bb->create_inst<CmpInst>(CmpInst::EQ, val,
                                               ZeroInit(BaseType::INT));
        if (val->get_type()->is<FloatType>())
            val = cur_bb->create_inst<CmpInst>(CmpInst::FEQ, val,
                                               ZeroInit(BaseType::FLOAT));
        break;
    }
    return val;
}
std::any visit(const RawVarDefStmt &node) { throw unreachable_error(); }
std::any visit(const RawFunDefGlobal &node) { throw unreachable_error(); }
std::any visit(const RawVarDefGlobal &node) { throw unreachable_error(); }