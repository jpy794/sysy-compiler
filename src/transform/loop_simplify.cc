#include "loop_simplify.hh"
#include "instruction.hh"
#include "utils.hh"

using namespace pass;
using namespace ir;
using namespace std;

auto LoopSimplify::split_phi_op(PhiInst *phi, const LoopInfo &loop)
    -> pair<vector<Pair>, vector<Pair>> {
    vector<Pair> inner, outer;
    for (auto &&[op, bb] : phi->to_pairs()) {
        if (contains(loop.bbs, bb)) {
            inner.emplace_back(op, bb);
        } else {
            outer.emplace_back(op, bb);
        }
    }
    return {inner, outer};
}

BasicBlock *LoopSimplify::create_preheader(BasicBlock *header,
                                           const LoopInfo &loop) {
    // assume all prebbs of loop body are within the loop itself
    // so here we only process prebb of header bb
    auto func = header->get_func();
    auto preheader = func->create_bb();

    // collect phi first (in case iterator invalidation)
    vector<PhiInst *> phis;
    for (auto &&inst : header->insts()) {
        if (not inst.is<PhiInst>()) {
            break;
        }
        phis.push_back(inst.as<PhiInst>());
    }

    // modify phi inst
    for (auto &&phi : phis) {
        auto [inner, outer] = split_phi_op(phi, loop);
        if (outer.size() == 0) {
            // all defs are from inside, do not modify
            continue;
        }
        if (inner.size() == 0) {
            // all defs are from outside, move to preheader
            preheader->move_inst(preheader->insts().begin(), phi);
            continue;
        }
        if (outer.size() == 1) {
            // only one defs from outside, set source to preheader
            inner.emplace_back(outer[0].first, preheader);
            phi->from_pairs(inner);
            continue;
        }
        // create a new phi in preheader
        auto phi_outer = preheader->insert_inst<PhiInst>(
            preheader->insts().begin(), phi->get_type());
        phi_outer->from_pairs(outer);
        inner.emplace_back(phi_outer, preheader);
        phi->from_pairs(inner);
    }

    // connect preheader to other bbs

    // to avoid iterator invalidation
    auto in_bbs = header->pre_bbs();
    for (auto in_bb : in_bbs) {
        if (not contains(loop.latches, in_bb)) {
            // connect in_bb to preheader
            in_bb->br_inst().replace_operand(header, preheader);
        }
    }

    // connect preheader to header
    preheader->create_inst<BrInst>(header);

    return preheader;
}

void LoopSimplify::create_exit(BasicBlock *exiting, BasicBlock *exit_target) {
    auto func = exiting->get_func();
    auto exit = func->create_bb();
    exiting->br_inst().replace_operand(exit_target, exit);
    exit->create_inst<BrInst>(exit_target);

    // phi
    for (auto &&inst : exit_target->insts()) {
        if (not inst.is<PhiInst>()) {
            break;
        }
        auto phi = inst.as<PhiInst>();
        for (size_t i = 0; i < phi->operands().size(); i++) {
            if (phi->get_operand(i) == exiting) {
                phi->set_operand(i, exit);
            }
        }
    }
}

void LoopSimplify::handle_func(Function *func, const FuncLoopInfo &loops) {
    for (auto &&[header, loop] : loops) {
        if (loop.preheader == nullptr) {
            create_preheader(header, loop);
        }
        for (auto [exiting, exit] : loop.exits) {
            if (exit == nullptr) {
                auto &bbs_in_loop = loop.bbs;
                auto exit_target =
                    find_if(exiting->suc_bbs().begin(),
                            exiting->suc_bbs().end(), [&](BasicBlock *bb) {
                                return not contains(bbs_in_loop, bb);
                            });
                assert(exit_target != exiting->suc_bbs().end());
                create_exit(exiting, *exit_target);
            }
        }
    }
}

void LoopSimplify::run(pass::PassManager *mgr) {
    auto &&loop_info = mgr->get_result<LoopFind>().loop_info;
    auto m = mgr->get_module();
    for (auto &&func : m->functions()) {
        if (func.is_external) {
            continue;
        }
        handle_func(&func, loop_info.at(&func));
    }
}
