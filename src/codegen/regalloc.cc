#include "regalloc.hh"
#include "mir_label.hh"
#include "mir_register.hh"
#include "utils.hh"
#include <algorithm>
#include <stack>
#include <string>
#include <vector>

using namespace std;
using namespace mir;
using namespace codegen;

void ControlFlowInfo::get_dfs_order(const mir::Function *func) {
    auto &labels = func->get_labels();
    assert(labels.size() != 0);

    stack<const Label *> todo;
    map<const Label *, bool> explored;

    todo.push(labels[0]);

    while (todo.size()) {
        auto label = todo.top();
        todo.pop();
        if (explored[label])
            continue;
        label_order.push_back(label);
        explored[label] = true;
        for (auto succ : label->get_succ())
            todo.push(succ);
    }
}

void ControlFlowInfo::get_inst_id() {
    unsigned id = 0;
    for (auto label : label_order)
        for (auto &inst : label->get_insts())
            instid.insert({&inst, id++});
}

LiveVarSet make_union(const vector<LiveVarSet> &sets) {
    LiveVarSet result;

    for (size_t i = 0; i < sets.size(); ++i) {
        LiveVarSet temp;
        set_union(result.begin(), result.end(), sets[i].begin(), sets[i].end(),
                  inserter(temp, temp.begin()));
        result = std::move(temp);
    }

    return result;
}

LiveVarSet make_intersection(const vector<LiveVarSet> &sets) {
    if (sets.empty()) {
        return LiveVarSet();
    }

    LiveVarSet result(sets[0].begin(), sets[0].end());
    for (size_t i = 1; i < sets.size(); ++i) {
        LiveVarSet temp;
        set_intersection(result.begin(), result.end(), sets[i].begin(),
                         sets[i].end(), inserter(temp, temp.begin()));
        result = std::move(temp);
    }

    return result;
}

LivenessAnalysis::LivenessAnalysis(const ControlFlowInfo &cfg_info,
                                   bool want_float) {
    auto &inst_id = cfg_info.instid;
    auto &label_order = cfg_info.label_order;

    reset(inst_id.size());
    auto IN_OF_INST = [&](const Instruction *inst) -> LiveVarSet & {
        return live_info.at(IN_POINT(inst_id.at(inst)));
    };
    auto OUT_OF_INST = [&](const Instruction *inst) -> LiveVarSet & {
        return live_info.at(OUT_POINT(inst_id.at(inst)));
    };
    auto IN_OF_LABEL = [&](const Label *label) -> LiveVarSet & {
        return IN_OF_INST(&label->get_insts().front());
    };
    auto OUT_OF_LABEL = [&](const Label *label) -> LiveVarSet & {
        return OUT_OF_INST(&label->get_insts().back());
    };

    vector<LiveVarSet> live_var_sets;
    while (1) {
        bool changed = false;
        // traverse each label
        for (auto label_it = label_order.rbegin();
             label_it != label_order.rend(); ++label_it) {
            live_var_sets.clear();

            auto label = *label_it;
            // collect successors' IN set
            for (auto succ : label->get_succ()) {
                live_var_sets.push_back(IN_OF_LABEL(succ));
            }
            OUT_OF_LABEL(label) = make_union(live_var_sets);

            // traverse each instruction
            for (auto inst_it = label->get_insts().rbegin();
                 inst_it != label->get_insts().rend(); ++inst_it) {
                auto inst = &*inst_it;

                // out-set
                if (inst_it != label->get_insts().rbegin()) {
                    // cur inst's out-set is the next inst's in-set
                    OUT_OF_INST(inst) =
                        live_info.at(OUT_POINT(inst_id.at(inst)) + 1);
                }

                // in-set
                auto new_in = OUT_OF_INST(inst);
                if (inst->will_write_register()) { // deal with def
                    auto op0 = inst->get_operand(0);
                    if (not want_float and is_a<const IVReg>(op0)) {
                        new_in.erase(as_a<const IVReg>(op0)->get_id());
                    } else if (want_float and is_a<const FVReg>(op0)) {
                        new_in.erase(as_a<const FVReg>(op0)->get_id());
                    }
                }
                LiveVarSet use; // deal with use
                for (auto i = (inst->will_write_register() ? 1 : 0);
                     i < inst->get_operand_num(); ++i) {
                    auto op = inst->get_operand(i);
                    if (not want_float and is_a<const IVReg>(op)) {
                        use.insert(as_a<const IVReg>(op)->get_id());
                    } else if (want_float and is_a<const FVReg>(op)) {
                        use.insert(as_a<const FVReg>(op)->get_id());
                    }
                }
                new_in = make_union({new_in, use});

                changed |= (new_in != IN_OF_INST(inst));
                IN_OF_INST(inst) = new_in;
            }
        }
        if (not changed)
            break;
    }
}

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
            {func, LivenessAnalysis(_cfg_info.at(func), false)});
        _liveness_float.insert(
            {func, LivenessAnalysis(_cfg_info.at(func), true)});

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
