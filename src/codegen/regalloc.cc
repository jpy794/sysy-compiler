#include "regalloc.hh"
#include "mir_label.hh"
#include "mir_register.hh"
#include "utils.hh"
#include <algorithm>
#include <string>
#include <vector>

using namespace std;
using namespace mir;
using namespace codegen;

RegAlloc::RegAlloc() {
    auto &reg_mgr = PhysicalRegisterManager::get();

    for (unsigned i = 0; i <= 7; ++i) // a{}
        _reg_pool_int.emplace(0, reg_mgr.a(i));
    for (unsigned i = 0; i <= 11; ++i) // s{}
        _reg_pool_int.emplace(1, reg_mgr.saved(i));
    for (unsigned i = 0; i <= 6; ++i) // t{}
        _reg_pool_int.emplace(2, reg_mgr.temp(i));
    _reg_pool_int.push({3, reg_mgr.ra()}); // ra

    for (unsigned i = 0; i <= 7; ++i) // fa{}
        _reg_pool_float.emplace(0, reg_mgr.fa(i));
    for (unsigned i = 0; i <= 11; ++i) // fs{}
        _reg_pool_float.emplace(1, reg_mgr.fsaved(i));
    for (unsigned i = 0; i <= 11; ++i) // ft{}
        _reg_pool_float.emplace(2, reg_mgr.ftemp(i));
}

void RegAlloc::run(const mir::Module *m) {
    clear();
    for (auto func : m->get_functions()) {
        if (not func->is_definition())
            continue;
        // liveness analysis
        _cfg_info.insert({func, ControlFlowInfo(func)});
        _liveness_int.insert(
            {func, LivenessAnalysis(_cfg_info.at(func), false, false)});
        _liveness_float.insert(
            {func, LivenessAnalysis(_cfg_info.at(func), true, false)});

        // reg alloction
        auto impl_int =
            new LinearScanImpl{_reg_pool_int, make_interval(func, false)};
        impl_int->run();
        _impl_int[func].reset(impl_int);
        auto impl_float =
            new LinearScanImpl{_reg_pool_float, make_interval(func, true)};
        impl_float->run();
        _impl_float[func].reset(impl_float);
    }
}

LinearScanImpl::LiveInts RegAlloc::make_interval(mir::Function *func,
                                                 bool for_float) {
    map<Register::RegIDType, LiveInterVal> ints; // vreg's id start from 1
    auto &liveness = get_liveness(func, for_float);
    for (auto arg : func->get_args()) {
        if ((for_float and is_a<FVReg>(arg)) or
            (not for_float and is_a<IVReg>(arg))) {
            auto vreg_id = arg->get_id();
            ints.emplace(vreg_id, vreg_id);
            ints.at(vreg_id).update(0);
        }
    }
    for (ProgramPoint i = 0; i < liveness.size(); ++i) {
        for (auto vreg_id : liveness[i]) {
            if (not contains(ints, vreg_id))
                ints.emplace(vreg_id, vreg_id);
            ints.at(vreg_id).update(i);
        }
    }
    LinearScanImpl::LiveInts res;
    for (auto [_, interval] : ints) {
        if (interval.check())
            res.insert(interval);
    }
    return res;
}
