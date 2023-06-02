#include <cstddef>
#pragma clang diagnostic ignored "-Wunused-variable"

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

using ICmpOp = ICmpInst::ICmpOp;
using FCmpOp = FCmpInst::FCmpOp;
using IBinOp = IBinaryInst::IBinOp;
using FBinOp = FBinaryInst::FBinOp;

Constant *CONST_INT(int x) { return Constants::get().int_const(x); }
Constant *CONST_FlOAT(int x) { return Constants::get().float_const(x); }

Type *get_arr_type(Type *base_type, vector<size_t> dims) {
    auto arr_ty = base_type;
    for (auto iter = dims.rbegin(); iter != dims.rend(); ++iter) {
        arr_ty = Types::get().array_type(arr_ty, *iter);
    }
    return arr_ty;
}

int main() {
    auto mod = new Module("test ir");
    auto &types = Types::get();
    auto &consts = Constants::get();
    auto inttype = Types::get().int_type();

    vector<std::pair<size_t, Constant *>> init;

    // int a[3][4][2] = {{}, 3, 4};
    init.clear();
    init.push_back({8, CONST_INT(3)});
    init.push_back({9, CONST_INT(4)});
    auto arr_type = get_arr_type(inttype, {3, 4, 2});
    mod->create_global_var(arr_type, "a", init);

    // int b[5][5] = {{}, {}, 1, 2, 3};
    init.clear();
    init.push_back({10, CONST_INT(1)});
    init.push_back({11, CONST_INT(2)});
    init.push_back({12, CONST_INT(3)});
    arr_type = get_arr_type(inttype, {5, 50});
    mod->create_global_var(arr_type, "b", init);

    // 65_color
    // int dp[maxn][maxn][maxn][maxn][maxn][7];
    const int maxn = 18;
    init.clear();
    arr_type = get_arr_type(inttype, {maxn, maxn, maxn, maxn, maxn, 7});
    mod->create_global_var(arr_type, "dp", init);

    // 84_long_array2
    // int c[7][5] = {{}, {1}, {2, 3}, {4, 5, 6}};
    init.clear();
    init.push_back({5, CONST_INT(1)});
    init.push_back({10, CONST_INT(2)});
    init.push_back({11, CONST_INT(3)});
    init.push_back({15, CONST_INT(4)});
    init.push_back({16, CONST_INT(5)});
    init.push_back({17, CONST_INT(6)});
    arr_type = get_arr_type(inttype, {7, 5});
    mod->create_global_var(arr_type, "c", init);

    // 05_arr_defn4
    // int d[4][2] = {{1, 2}, {3, 4}, {}, 7}
    init.clear();
    init.push_back({0, CONST_INT(1)});
    init.push_back({1, CONST_INT(2)});
    init.push_back({2, CONST_INT(3)});
    init.push_back({3, CONST_INT(4)});
    init.push_back({6, CONST_INT(7)});
    arr_type = get_arr_type(inttype, {4, 2});
    mod->create_global_var(arr_type, "d", init);

    // int e[5][4][2] = {{}, 1, 2, {}, 5, 6, 7, 8, {1}};
    init.clear();
    init.push_back({8, CONST_INT(1)});
    init.push_back({9, CONST_INT(2)});
    init.push_back({12, CONST_INT(5)});
    init.push_back({13, CONST_INT(6)});
    init.push_back({14, CONST_INT(7)});
    init.push_back({15, CONST_INT(8)});
    init.push_back({16, CONST_INT(1)});
    arr_type = get_arr_type(inttype, {5, 4, 2});
    mod->create_global_var(arr_type, "e", init);


    // main函数
    auto mainFunType = types.func_type(inttype, {});
    auto mainFun = mod->create_func(mainFunType, "main");
    auto bb = mainFun->create_bb();

    bb->create_inst<RetInst>(CONST_INT(0));
    cout << mod->print();
    delete mod;
    return 0;
}
