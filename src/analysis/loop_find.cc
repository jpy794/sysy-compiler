#include "loop_find.hh"
#include "basic_block.hh"
#include "dominator.hh"
#include "err.hh"
#include "instruction.hh"
#include "log.hh"
#include "utils.hh"
#include <queue>
#include <sys/types.h>
#include <unordered_map>

using namespace pass;
using namespace std;
using namespace ir;

bool LoopFind::run(PassManager *mgr) {
    using LoopInfo = ResultType::LoopInfo;

    clear();

    _dom = &mgr->get_result<Dominator>();
    _m = mgr->get_module();

    for (auto &&func : _m->functions()) {
        if (func.is_external) {
            continue;
        }
        // for each bb in func to find all loops in func
        unordered_map<BasicBlock *, LoopInfo> loops;
        for (auto &&bb : func.bbs()) {
            // try find a loop using bb as header
            for (auto &&pre_bb : bb.pre_bbs()) {
                if (_dom->is_dom(&bb, pre_bb)) {
                    // found a latch pre_bb -> bb
                    if (not contains(loops, &bb)) {
                        // first time to find the header bb
                        loops.insert({&bb, LoopInfo{}});
                    }
                    auto &&loop = loops[&bb];
                    loop.latches.push_back(pre_bb);
                    loop.bbs.merge(find_bbs_by_latch(&bb, pre_bb));
                }
            }
            if (contains(loops, &bb)) {
                auto &loop = loops[&bb];

                // set preheader if exists
                vector<BasicBlock *> outer_pre_bbs;
                for (auto pre : bb.pre_bbs()) {
                    // prebb out of the loop that has only one
                    // sucbb (header)
                    if (not contains(loop.bbs, pre)) {
                        outer_pre_bbs.push_back(pre);
                    }
                }

                loop.preheader = nullptr;
                if (outer_pre_bbs.size() == 1 and
                    outer_pre_bbs.back()->suc_bbs().size() == 1) {
                    loop.preheader = outer_pre_bbs.back();
                }

                // find all exiting bbs
                // set exit bb for each exiting bb if exists
                for (auto bb : loop.bbs) {
                    for (auto suc : bb->suc_bbs()) {
                        if (not contains(loop.bbs, suc)) {
                            loop.exits.insert({bb, nullptr});
                            if (suc->pre_bbs().size() == 1) {
                                loop.exits[bb] = suc;
                            }
                        }
                    }
                }

                // set sub loop
                for (auto &&[other_header, other_loop] : loops) {
                    if (other_header == &bb) {
                        continue;
                    }
                    if (contains(other_loop.bbs, &bb)) {
                        other_loop.sub_loops.insert(&bb);
                    }
                }

                // parse ind var
                loop.ind_var_info = parse_ind_var(&bb, loop);
            }
        }
        _result.loop_info[&func].loops = std::move(loops);
    }
    // log();
    return false;
}

set<BasicBlock *> LoopFind::find_bbs_by_latch(BasicBlock *header,
                                              BasicBlock *latch) {
    set<BasicBlock *> ret;
    ret.insert(header);
    queue<BasicBlock *> bfs;
    bfs.push(latch);
    ret.insert(latch);
    while (not bfs.empty()) {
        auto bb = bfs.front();
        bfs.pop();
        if (bb == header) {
            continue;
        }
        for (auto &&pre_bb : bb->pre_bbs()) {
            if (not contains(ret, pre_bb)) {
                bfs.push(pre_bb);
                ret.insert(pre_bb);
            }
        }
    }
    return ret;
}

auto LoopFind::parse_ind_var(BasicBlock *header, const LoopInfo &loop)
    -> optional<LoopInfo::IndVarInfo> {

    LoopInfo::IndVarInfo ind_var_info;

    if (loop.latches.size() > 1) {
        return nullopt;
    }

    // the loop has too many exiting edges
    if (loop.exits.size() > 1) {
        return nullopt;
    }

    // the exiting bb is not header
    auto exiting = loop.exits.begin()->first;
    if (exiting != header) {
        return nullopt;
    }

    // induction var
    auto br_inst = header->insts().back().as<BrInst>();
    assert(br_inst->operands().size() == 3);
    auto cond = br_inst->get_operand(0);
    // the induction variable should be of int type
    if (not cond->is<ICmpInst>()) {
        return nullopt;
    }

    auto icmp_inst = cond->as<ICmpInst>();
    auto icmp_op = icmp_inst->get_icmp_op();
    auto lhs = icmp_inst->lhs();
    auto rhs = icmp_inst->rhs();

    auto exit_cond = [&](bool is_ind_rhs) {
        auto opposite = ICmpInst::opposite_icmp_op(icmp_op);
        auto op = is_ind_rhs ? opposite : icmp_op;
        if (not contains(loop.bbs, br_inst->get_operand(2)->as<BasicBlock>())) {
            // exit if cond is true
            return op;
        } else if (not contains(loop.bbs,
                                br_inst->get_operand(1)->as<BasicBlock>())) {
            // exit if cond is false
            return ICmpInst::not_icmp_op(op);
        } else {
            throw unreachable_error{};
        }
    };

    auto is_loop_invariant = [&](Value *v) {
        if (not v->is<Instruction>()) {
            return true;
        }
        auto inst = as_a<Instruction>(v);
        return not contains(loop.bbs, inst->get_parent());
    };

    if (is_loop_invariant(lhs)) {
        ind_var_info.ind_var = rhs;
        ind_var_info.icmp_op = exit_cond(true);
        ind_var_info.bound = lhs;
    } else if (is_loop_invariant(rhs)) {
        ind_var_info.ind_var = lhs;
        ind_var_info.icmp_op = exit_cond(false);
        ind_var_info.bound = rhs;
    } else {
        return nullopt;
    }

    // find initial and step
    ind_var_info.initial = nullptr;
    ind_var_info.step = nullptr;
    for (auto &&inst : header->insts()) {
        if (not inst.is<PhiInst>()) {
            break;
        }
        if (&inst != ind_var_info.ind_var) {
            continue;
        }
        // the phi for induction variable
        auto phi_inst = inst.as<PhiInst>();
        // the phi should has two source, one from preheader and the other from
        // loop body
        if (phi_inst->to_pairs().size() != 2) {
            return nullopt;
        }
        for (auto [value, source] : phi_inst->to_pairs()) {
            if (contains(loop.bbs, source)) {
                // step
                if (not value->is<IBinaryInst>()) {
                    continue;
                }
                auto ibinary_inst = value->as<IBinaryInst>();
                auto ibin_op = ibinary_inst->get_ibin_op();
                if (ibin_op != IBinaryInst::ADD) {
                    continue;
                }
                if (ibinary_inst->rhs() == ind_var_info.ind_var) {
                    ind_var_info.step = ibinary_inst->lhs();
                } else if (ibinary_inst->lhs() == ind_var_info.ind_var) {
                    ind_var_info.step = ibinary_inst->rhs();
                }
            } else {
                // initial
                ind_var_info.initial = value;
            }
        }
    }
    if (ind_var_info.initial == nullptr or ind_var_info.step == nullptr) {
        return nullopt;
    }
    return ind_var_info;
}

vector<BasicBlock *>
LoopFind::ResultType::FuncLoopInfo::get_topo_order() const {
    vector<BasicBlock *> ret;
    map<BasicBlock *, size_t> parent_cnt;
    for (auto &&[header, _] : loops) {
        parent_cnt[header] = 0;
    }
    for (auto &&[_, loop] : loops) {
        for (auto sub_header : loop.sub_loops) {
            parent_cnt[sub_header]++;
        }
    }
    while (not parent_cnt.empty()) {
        bool changed{false};
        for (auto &&[header, cnt] : parent_cnt) {
            if (cnt == 0) {
                ret.push_back(header);
                for (auto sub_header : loops.at(header).sub_loops) {
                    parent_cnt[sub_header]--;
                }
                parent_cnt.erase(header);
                changed = true;
                break;
            }
        }
        assert(changed);
    }
    return ret;
}

void LoopFind::log() const {
    for (auto &&[func, func_loop] : _result.loop_info) {
        debugs << func->get_name() << '\n';
        for (auto &&[header, loop] : func_loop.loops) {
            debugs << "loop " << header->get_name() << '\n';
            for (auto &&latch : loop.latches) {
                debugs << latch->get_name() << ' ';
            }
            debugs << '\n';
            for (auto &&bb : loop.bbs) {
                debugs << bb->get_name() << ' ';
            }
            debugs << "\n\n";
        }
        debugs << '\n';
    }
}
