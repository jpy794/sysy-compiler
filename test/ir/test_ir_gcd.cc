#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "constant.hh"
#include "function.hh"
#include "global_variable.hh"
#include "ircollector.hh"
#include "module.hh"
#include "type.hh"
#include "value.hh"

/* #ifdef DEBUG // 用于调试信息,大家可以在编译过程中通过" -DDEBUG"来开启这一选项
 * #define DEBUG_OUTPUT cout << __LINE__ << endl; //
 * 输出行号的简单示例 #else #define DEBUG_OUTPUT #endif */

#define CONST_INT(num) (mod->get_const_int(num))

// 得到常数值的表示,方便后面多次用到
#define CONST_FP(num) (mod->get_const_float(num))

using namespace ir;
using namespace std;

int main() {
    auto mod = new Module("test ir");
    auto builder = new IRCollector(mod);

    auto i32type = mod->get_int32_type();

    // 全局数组,x,y
    vector<int> zero_vec{0};
    auto *arrayType = mod->get_array_type(i32type, {1});
    auto initializer = ConstantArray::get(zero_vec, mod);
    auto x = GlobalVariable::get(arrayType, initializer, "x", mod);
    auto y = GlobalVariable::get(arrayType, initializer, "y", mod);

    // gcd函数
    // 函数参数类型的vector
    vector<Type *> param_type(2, i32type);
    // 通过返回值类型与参数类型列表得到函数类型
    auto gcdFunTy = mod->get_function_type(
        i32type, static_cast<decltype(param_type) &&>(param_type));
    // 由函数类型得到函数
    auto gcdFun = Function::create(gcdFunTy, "gcd", mod);

    // 创建函数
    auto entry = BasicBlock::create(gcdFun);
    builder->set_insertion(entry);

    // 为ret分配内存
    auto retAlloca = builder->create_alloc(i32type);
    // 参数u, v
    auto u = gcdFun->get_args()[0];
    auto v = gcdFun->get_args()[1];

    auto icmp = builder->create_cmp_eq(v, CONST_INT(0));
    auto TBB = BasicBlock::create(gcdFun); // true分支
    auto FBB = BasicBlock::create(gcdFun); // false分支
    auto retBB = BasicBlock::create(gcdFun);
    builder->create_cond_br(icmp, TBB, FBB);

    // if true; 分支的开始需要SetInsertPoint设置
    builder->set_insertion(TBB);
    builder->create_store(u, retAlloca);
    // TODO: 写builder的时候，要检查块的最后一条指令是跳转
    builder->create_br(retBB);

    // if false
    builder->set_insertion(FBB);
    auto div = builder->create_sdiv(u, v);
    auto mul = builder->create_mul(div, v);
    auto sub = builder->create_sub(u, mul);
    auto call = builder->create_call(gcdFun, {v, sub});

    builder->create_store(call, retAlloca);
    builder->create_br(retBB); // br retBB

    builder->set_insertion(retBB); // ret分支
    auto retLoad = builder->create_load(retAlloca);
    builder->create_ret(retLoad);

    // funArray函数
    auto i32ptrtype = mod->get_pointer_type(i32type);
    auto funArrayFunType =
        mod->get_function_type(i32type, {i32ptrtype, i32ptrtype});
    auto funArrayFun = Function::create(funArrayFunType, "funArray", mod);
    entry = BasicBlock::create(funArrayFun);
    builder->set_insertion(entry);
    auto aAlloca = builder->create_alloc(i32type);
    auto bAlloca = builder->create_alloc(i32type);
    auto tempAlloca = builder->create_alloc(i32type);
    u = funArrayFun->get_args()[0];
    v = funArrayFun->get_args()[1];

    // TODO: not correct, should use gep here
    auto uGEP = builder->create_gep(u, {CONST_INT(0)});
    auto vGEP = builder->create_gep(v, {CONST_INT(0)});
    auto u0 = builder->create_load(uGEP);
    auto v0 = builder->create_load(vGEP);
    builder->create_store(u0, aAlloca);
    builder->create_store(v0, bAlloca);

    auto aLoad = builder->create_load(aAlloca);
    auto bLoad = builder->create_load(bAlloca);
    icmp = builder->create_cmp_lt(aLoad, bLoad);
    TBB = BasicBlock::create(funArrayFun);
    FBB = BasicBlock::create(funArrayFun);
    builder->create_cond_br(icmp, TBB, FBB);

    builder->set_insertion(TBB);
    builder->create_store(aLoad, tempAlloca);
    builder->create_store(bLoad, aAlloca);
    auto tempLoad = builder->create_load(tempAlloca);
    builder->create_store(tempLoad, bAlloca);
    builder->create_br(FBB);

    builder->set_insertion(FBB);
    aLoad = builder->create_load(aAlloca);
    bLoad = builder->create_load(bAlloca);
    call = builder->create_call(gcdFun, {aLoad, bLoad});
    builder->create_ret(call);

    // main函数
    auto mainFun =
        Function::create(mod->get_function_type(i32type, {}), "main", mod);
    entry = BasicBlock::create(mainFun);
    builder->set_insertion(entry);
    retAlloca = builder->create_alloc(i32type);

    auto xGEP = builder->create_gep(x, {CONST_INT(0), CONST_INT(0)});
    auto yGEP = builder->create_gep(y, {CONST_INT(0), CONST_INT(0)});
    builder->create_store(CONST_INT(90), xGEP);
    builder->create_store(CONST_INT(18), yGEP);

    call = builder->create_call(funArrayFun, {xGEP, yGEP});

    builder->create_ret(call);
    cout << mod->print();
    delete mod;
    return 0;
}
