#include "liveness.hh"
#include "mir_config.hh"
#include "mir_function.hh"
#include "mir_instruction.hh"
#include "mir_register.hh"

#include <stack>

using namespace std;
using namespace mir;
using namespace codegen;

static auto preg_mgr = PhysicalRegisterManager::get();

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

void resolve_pesudo_use(const Function *func, const Instruction *inst,
                        LiveVarSet &use, bool want_float) {
    switch (inst->get_opcode()) {
    case Call: { // use argument reg
        assert(is_a<const Function>(inst->get_operand(0)));
        unsigned i = 0;
        for (auto arg :
             as_a<const Function>(inst->get_operand(0))->get_args()) {
            if (want_float and arg->is_float_reg())
                use.insert(preg_mgr.farg(i++)->get_id());
            else if (not want_float and arg->is_int_reg())
                use.insert(preg_mgr.arg(i++)->get_id());
            if (i == 8)
                break;
        }
        break;
    }
    case Ret: // use return value
        if (func->get_ret_type() == BasicType::INT and not want_float)
            use.insert(preg_mgr.a(0)->get_id());
        else if (func->get_ret_type() == BasicType::FLOAT and want_float)
            use.insert(preg_mgr.fa(0)->get_id());
        break;
    case COMMENT: // pass live varset only
        use.clear();
        break;
    default:
        break;
    }
}

void LivenessAnalysis::run(const ControlFlowInfo &cfg_info, bool want_float,
                           bool pesudo) {
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

                // get out-set
                if (inst_it != label->get_insts().rbegin()) {
                    // cur inst's out-set is the next inst's in-set
                    OUT_OF_INST(inst) =
                        live_info.at(OUT_POINT(inst_id.at(inst)) + 1);
                }
                // get in-set
                auto new_in = OUT_OF_INST(inst);

                // deal with def
                const Register *def = nullptr;
                if (inst->will_write_register()) {
                    auto op0 = inst->get_operand(0);
                    if ((not want_float and op0->is_int_reg()) or
                        (want_float and op0->is_float_reg())) {
                        def = as_a<const Register>(op0);
                    }
                }
                // deal with use
                LiveVarSet use;
                for (auto i = (inst->will_write_register() ? 1 : 0);
                     i < inst->get_operand_num(); ++i) {
                    auto op = inst->get_operand(i);
                    if ((not want_float and op->is_int_reg()) or
                        (want_float and op->is_float_reg())) {
                        use.insert(as_a<const Register>(op)->get_id());
                    }
                }
                // take care of pesudo side-effect on use
                if (pesudo)
                    resolve_pesudo_use(cfg_info.func, inst, use, want_float);

                // ignore x0, beacause x0 will
                if (not want_float)
                    use.erase(preg_mgr.zero()->get_id());
                if (def and def != preg_mgr.zero())
                    new_in.erase(def->get_id());

                new_in = make_union({new_in, use});

                changed |= (new_in != IN_OF_INST(inst));
                IN_OF_INST(inst) = new_in;
            }
        }
        if (not changed)
            break;
    }
}
