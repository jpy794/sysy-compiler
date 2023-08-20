#pragma once

#include "dead_code.hh"
#include "function.hh"
#include "instruction.hh"
#include "log.hh"
#include "pass.hh"
#include "type.hh"

namespace pass {

class FuncTrim final : public TransformPass {

  public:
    FuncTrim() = default;
    virtual void get_analysis_usage(pass::AnalysisUsage &AU) const override {
        TransformPass::get_analysis_usage(AU);
        AU.add_post<pass::DeadCode>();
    }
    bool run(pass::PassManager *mgr) override {
        bool changed = false;
        for (auto &func : mgr->get_module()->functions()) {
            if (&func == mgr->get_module()->get_main() or func.is_external)
                continue;
            changed |= ret_decay(&func);
        }
        return changed;
    }

  private:
    bool ret_decay(ir::Function *func) {
        if (func->get_type()
                ->as<ir::FuncType>()
                ->get_result_type()
                ->is<ir::VoidType>())
            return false;
        bool all_use_is_useless = true;
        for (auto &[use, _] : func->get_use_list()) {
            if (use->get_use_list().size()) {
                all_use_is_useless = false;
                break;
            }
        }
        if (all_use_is_useless) {
            func->decay_to_void_ret();
            for (auto &[use, _] : func->get_use_list()) {
                as_a<ir::CallInst>(use)->decay_to_void_type();
            }
        }
        if (all_use_is_useless)
            debugs << "trim function: " << func->get_name() << "\n";
        return all_use_is_useless;
    }
};

} // namespace pass
