#include "regalloc.hh"
#include "mir_label.hh"
#include <stack>

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

inline ProgramPoint IN_POINT(InstructionID id) { return 2 * id; }
inline ProgramPoint OUT_POINT(InstructionID id) { return 2 * id + 1; }

LivenessAnalysis::LivenessAnalysis(const ControlFlowInfo &cfg_info,
                                   bool integer) {
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
                    if (integer and is_a<const IVReg>(op0)) {
                        new_in.erase(as_a<const IVReg>(op0)->get_id());
                    } else if (not integer and is_a<const FVReg>(op0)) {
                        new_in.erase(as_a<const FVReg>(op0)->get_id());
                    }
                }
                LiveVarSet use; // deal with use
                for (auto i = (inst->will_write_register() ? 2 : 1);
                     i < inst->get_operand_num(); ++i) {
                    auto op = inst->get_operand(i);
                    if (integer and is_a<const IVReg>(op)) {
                        use.insert(as_a<const IVReg>(op)->get_id());
                    } else if (not integer and is_a<const FVReg>(op)) {
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
