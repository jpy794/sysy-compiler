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

using IBinOp = IBinaryInst::IBinOp;

// int a[2] = {1, 2}
// int main() { return a[0] + a[1]; }
int main() {
    auto mod = new Module("test ir");
    auto &types = Types::get();
    auto &consts = Constants::get();
    auto inttype = Types::get().int_type();
    // auto i64type = Types::get().i64_int_type();

    // 全局数组 a
    auto init = Constants::get().array_const({CONST_INT(1), CONST_INT(2)});
    auto arrayType = types.array_type(inttype, 2);
    auto a = mod->create_global_var(arrayType, "a", init);

    // main函数
    auto mainFunType = types.func_type(inttype, {});
    auto mainFun = mod->create_func(mainFunType, "main");
    auto bb = mainFun->create_bb();

    // old method
    /* auto a_0_addr =
     *     bb->create_inst<GetElementPtrInst>(a, CONST_INT(0), CONST_INT(0));
     * auto a_0 = bb->create_inst<LoadInst>(a_0_addr);
     * auto a_1_addr =
     *     bb->create_inst<GetElementPtrInst>(a, CONST_INT(0), CONST_INT(1));
     * auto a_1 = bb->create_inst<LoadInst>(a_1_addr); */

    // new method
    auto base_address = bb->create_inst<Ptr2IntInst>(a);
    auto a_0_addr = bb->create_inst<Int2PtrInst>(base_address, inttype);
    auto a_0 = bb->create_inst<LoadInst>(a_0_addr);
    auto a_1_addr_val = bb->create_inst<IBinaryInst>(IBinOp::ADD, base_address,
                                                     consts.i64_const(4));
    auto a_1_addr = bb->create_inst<Int2PtrInst>(a_1_addr_val, inttype);
    auto a_1 = bb->create_inst<LoadInst>(a_1_addr);

    auto sum = bb->create_inst<IBinaryInst>(IBinOp::ADD, a_0, a_1);

    bb->create_inst<RetInst>(sum);
    cout << mod->print();
    delete mod;
    return 0;
}
