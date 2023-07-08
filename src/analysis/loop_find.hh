#pragma once

#include "dominator.hh"
#include "instruction.hh"
#include "module.hh"
#include "pass.hh"
#include <unordered_map>
#include <vector>

namespace pass {

class LoopFind final : public pass::AnalysisPass {
  public:
    struct ResultType {
        struct LoopInfo {
            std::vector<ir::BasicBlock *> latches;
            std::set<ir::BasicBlock *> bbs;
        };
        // ((func, ((header, loop_info)...))...)
        std::unordered_map<ir::Function *,
                           std::unordered_map<ir::BasicBlock *, LoopInfo>>
            loop_info;
    };

    virtual void get_analysis_usage(pass::AnalysisUsage &AU) const final {
        using KillType = pass::AnalysisUsage::KillType;
        AU.set_kill_type(KillType::None);
        AU.add_require<pass::Dominator>();
    }

    virtual std::any get_result() const final { return &_result; }

    virtual void run(pass::PassManager *mgr) final;

    virtual void clear() final {
        _result.loop_info.clear();
        _dom = nullptr;
        _m = nullptr;
    }

  private:
    std::set<ir::BasicBlock *> find_bbs_by_latch(ir::BasicBlock *header,
                                                 ir::BasicBlock *latch);
    void log() const;

  private:
    ResultType _result;
    const Dominator::ResultType *_dom{nullptr};
    ir::Module *_m{nullptr};
};

} // namespace pass