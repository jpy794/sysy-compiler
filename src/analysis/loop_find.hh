#pragma once

#include "basic_block.hh"
#include "dominator.hh"
#include <optional>
#include <unordered_map>
#include <vector>

namespace pass {

class LoopFind final : public AnalysisPass {
  public:
    struct ResultType {
        struct LoopInfo {
            std::vector<ir::BasicBlock *> latches;
            std::set<ir::BasicBlock *> bbs;
            ir::BasicBlock *preheader;
            std::map<ir::BasicBlock *, ir::BasicBlock *> exits;
            std::set<ir::BasicBlock *> sub_loops;
            struct IndVarInfo {
                ir::Value *ind_var, *initial, *step, *bound;
                ir::ICmpInst::ICmpOp icmp_op;
            };
            std::optional<IndVarInfo> ind_var_info{std::nullopt};
        };
        struct FuncLoopInfo {
            std::unordered_map<ir::BasicBlock *, LoopInfo> loops;
            std::vector<ir::BasicBlock *> get_topo_order() const;
        };
        // ((func, ((header, loop_info)...))...)
        std::unordered_map<ir::Function *, FuncLoopInfo> loop_info;
    };

    void get_analysis_usage(AnalysisUsage &AU) const final {
        using KillType = AnalysisUsage::KillType;
        AU.set_kill_type(KillType::None);
        AU.add_require<Dominator>();
    }

    std::any get_result() const final { return &_result; }

    bool run(PassManager *mgr) final;

    void clear() final {
        _result.loop_info.clear();
        _dom = nullptr;
        _m = nullptr;
    }

  private:
    using LoopInfo = ResultType::LoopInfo;

    std::set<ir::BasicBlock *> find_bbs_by_latch(ir::BasicBlock *header,
                                                 ir::BasicBlock *latch);
    auto parse_ind_var(ir::BasicBlock *header, const LoopInfo &loop)
        -> std::optional<LoopInfo::IndVarInfo>;
    void log() const;

    ResultType _result;
    const Dominator::ResultType *_dom{nullptr};
    ir::Module *_m{nullptr};
};

} // namespace pass
