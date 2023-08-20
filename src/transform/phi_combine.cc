#include "phi_combine.hh"
#include "instruction.hh"
#include "utils.hh"

using namespace std;
using namespace pass;
using namespace ir;

bool PhiCombine::run(PassManager *mgr) {
    auto m = mgr->get_module();
    for (auto &&func : m->functions()) {
        if (func.is_external) {
            continue;
        }
        handle_func(&func);
    }
    return false;
}

void PhiCombine::handle_func(Function *func) {
    auto changed{true};
    while (changed) {
        changed = false;
        for (auto &&bb : func->bbs()) {
            for (auto pre : bb.pre_bbs()) {
                if (try_combine(&bb, pre)) {
                    changed = true;
                    goto loop_end;
                }
            }
        }
    loop_end:;
    }
}

bool PhiCombine::try_combine(BasicBlock *bb, BasicBlock *pre_bb) {
    assert(contains(bb->pre_bbs(), pre_bb));

    // collect phi
    vector<Value *> phis;
    for (auto &&inst : bb->insts()) {
        if (not inst.is<PhiInst>()) {
            break;
        }
        phis.push_back(&inst);
    }

    vector<Value *> pre_phis;
    for (auto &&inst : pre_bb->insts()) {
        if (not inst.is<PhiInst>()) {
            break;
        }
        for (auto &&user : inst.get_use_list()) {
            if (not contains(phis, static_cast<Value *>(user.user))) {
                // this phi is not only used by phi inst in phis
                // do not combine
                return false;
            }
        }
        pre_phis.push_back(&inst);
    }

    // no phis to combine
    if (phis.empty() or pre_phis.empty()) {
        return false;
    }

    // pre_bb doesn't consist only of phi insts and a terminator
    if (pre_phis.size() + 1 != pre_bb->insts().size()) {
        return false;
    }

    // now we're determined to combine the two bbs into one
    for (auto phi_val : phis) {
        auto phi = phi_val->as<PhiInst>();
        decltype(phi->to_pairs()) new_phi_pair;
        for (auto &&[value, bb] : phi->to_pairs()) {
            if (contains(pre_phis, value)) {
                auto pre_phi_pair = value->as<PhiInst>()->to_pairs();
                new_phi_pair.insert(new_phi_pair.end(), pre_phi_pair.begin(),
                                    pre_phi_pair.end());
            } else if (bb != pre_bb) {
                new_phi_pair.push_back({value, bb});
            } else {
                // clone phi src for each pre bb
                for (auto pre_pre : pre_bb->pre_bbs()) {
                    new_phi_pair.push_back({value, pre_pre});
                }
            }
        }
        phi->from_pairs(new_phi_pair);
    }

    // reconnect cfg
    // in case iterator invalidation
    auto pre_pre_bbs = pre_bb->pre_bbs();
    for (auto pre_pre : pre_pre_bbs) {
        pre_pre->br_inst().replace_operand(pre_bb, bb);
    }
    // erase pre_bb
    pre_bb->erase_inst(&pre_bb->br_inst());
    pre_bb->get_func()->erase_bb(pre_bb);

    return true;
}
