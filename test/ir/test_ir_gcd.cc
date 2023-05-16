#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "constant.hh"
#include "function.hh"
#include "global_variable.hh"
#include "instruction.hh"
#include "module.hh"
#include "type.hh"
#include "value.hh"

using namespace ir;
using namespace std;

Constant *CONST_INT(int x) { return Constants::get().int_const(x); }
Constant *CONST_FlOAT(int x) { return Constants::get().float_const(x); }

using ICmpOp = ICmpInst::ICmpOp;
using FCmpOp = FCmpInst::FCmpOp;
using IBinOp = IBinaryInst::IBinOp;
using FBinOp = FBinaryInst::FBinOp;

int main() {
    auto mod = new Module("test ir");
    auto &types = Types::get();
    auto &consts = Constants::get();
    auto inttype = Types::get().int_type();

    // 全局数组,x,y
    vector<int> zero_vec{0};
    auto arrayType = types.array_type(inttype, 1);
    auto initializer = consts.array_const({CONST_INT(0)});
    auto x = mod->create_global_var(arrayType, initializer,
                                    std::forward<string>("x"));
    auto y = mod->create_global_var(arrayType, initializer,
                                    std::forward<string>("y"));

    //
    // gcd函数
    // 函数参数类型的vector
    vector<Type *> param_type(2, inttype);
    // 通过返回值类型与参数类型列表得到函数类型
    auto gcdFunTy = types.func_type(inttype, std::move(param_type));
    // 由函数类型得到函数
    auto gcdFun = mod->create_func(gcdFunTy, std::forward<string>("gcd"));

    // 创建基本块
    auto entry = gcdFun->create_bb();
    auto bb = gcdFun->create_bb();
    entry->create_inst<BrInst>(bb);

    // 为ret分配内存
    auto retAlloca = bb->create_inst<AllocaInst>(inttype);
    // 参数u, v
    auto u = gcdFun->get_args()[0];
    auto v = gcdFun->get_args()[1];

    auto icmp = bb->create_inst<ICmpInst>(ICmpOp::EQ, v, CONST_INT(0));
    auto TBB = gcdFun->create_bb();
    auto FBB = gcdFun->create_bb();
    auto retBB = gcdFun->create_bb();
    bb->create_inst<BrInst>(icmp, TBB, FBB);

    // if true
    TBB->create_inst<StoreInst>(u, retAlloca);
    // TODO: 写builder的时候，要检查块的最后一条指令是跳转
    TBB->create_inst<BrInst>(retBB);

    // if false
    auto div = FBB->create_inst<IBinaryInst>(IBinOp::SDIV, u, v);
    auto mul = FBB->create_inst<IBinaryInst>(IBinOp::MUL, div, v);
    auto sub = FBB->create_inst<IBinaryInst>(IBinOp::SUB, u, mul);
    auto call = FBB->create_inst<CallInst>(gcdFun, v, sub);

    FBB->create_inst<StoreInst>(call, retAlloca);
    FBB->create_inst<BrInst>(retBB);

    // ret BB
    auto retLoad = retBB->create_inst<LoadInst>(retAlloca);
    retBB->create_inst<RetInst>(retLoad);

    // funArray函数
    auto intptrtype = types.ptr_type(inttype);
    auto funArrayFunType = types.func_type(inttype, {intptrtype, intptrtype});
    auto funArrayFun = mod->create_func(funArrayFunType, "funcArray");
    entry = funArrayFun->create_bb();
    // NOTE: 2 entry style, this is different from gcdFun
    auto aAlloca = entry->create_inst<AllocaInst>(inttype);
    auto bAlloca = entry->create_inst<AllocaInst>(inttype);
    auto tempAlloca = entry->create_inst<AllocaInst>(inttype);
    u = funArrayFun->get_args()[0];
    v = funArrayFun->get_args()[1];

    auto uGEP = entry->create_inst<GetElementPtrInst>(u, CONST_INT(0));
    auto vGEP = entry->create_inst<GetElementPtrInst>(v, CONST_INT(0));
    auto u0 = entry->create_inst<LoadInst>(uGEP);
    auto v0 = entry->create_inst<LoadInst>(vGEP);
    entry->create_inst<StoreInst>(u0, aAlloca);
    entry->create_inst<StoreInst>(v0, bAlloca);

    auto aLoad = entry->create_inst<LoadInst>(aAlloca);
    auto bLoad = entry->create_inst<LoadInst>(bAlloca);
    icmp = entry->create_inst<ICmpInst>(ICmpOp::LT, aLoad, bLoad);
    TBB = funArrayFun->create_bb();
    retBB = funArrayFun->create_bb();
    entry->create_inst<BrInst>(icmp, TBB, retBB);

    // if true
    TBB->create_inst<StoreInst>(aLoad, tempAlloca);
    TBB->create_inst<StoreInst>(bLoad, aAlloca);
    auto tempLoad = TBB->create_inst<LoadInst>(tempAlloca);
    TBB->create_inst<StoreInst>(tempLoad, bAlloca);
    TBB->create_inst<BrInst>(retBB);
    // if false, return directly
    aLoad = retBB->create_inst<LoadInst>(aAlloca);
    bLoad = retBB->create_inst<LoadInst>(bAlloca);
    call = retBB->create_inst<CallInst>(gcdFun, aLoad, bLoad);
    retBB->create_inst<RetInst>(call);

    // main函数
    auto mainFunType = types.func_type(inttype, {});
    auto mainFun = mod->create_func(mainFunType, "main");
    bb = mainFun->create_bb();

    vector<Constant *> off;
    auto xGEP =
        bb->create_inst<GetElementPtrInst>(x, CONST_INT(0), CONST_INT(0));
    auto yGEP =
        bb->create_inst<GetElementPtrInst>(y, CONST_INT(0), CONST_INT(0));
    bb->create_inst<StoreInst>(CONST_INT(90), xGEP);
    bb->create_inst<StoreInst>(CONST_INT(18), yGEP);

    call = bb->create_inst<CallInst>(funArrayFun, xGEP, yGEP);

    bb->create_inst<RetInst>(call);
    cout << mod->print();
    delete mod;
    return 0;
}
