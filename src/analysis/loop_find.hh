#pragma once

#include "basic_block.hh"
#include "dominator.hh"
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
    std::set<ir::BasicBlock *> find_bbs_by_latch(ir::BasicBlock *header,
                                                 ir::BasicBlock *latch);
    void log() const;

    ResultType _result;
    const Dominator::ResultType *_dom{nullptr};
    ir::Module *_m{nullptr};
};

} // namespace pass
