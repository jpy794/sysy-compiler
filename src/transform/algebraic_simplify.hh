#include "basic_block.hh"
#include "const_propagate.hh"
#include "dead_code.hh"
#include "functional"
#include "instruction.hh"
#include "pass.hh"
#include <functional>
#include <list>

namespace pass {

class AlgebraicSimplify : public TransformPass {
  public:
    AlgebraicSimplify() = default;
    void get_analysis_usage(AnalysisUsage &AU) const override final {
        using KillType = AnalysisUsage::KillType;
        AU.set_kill_type(KillType::All);
        AU.add_post<DeadCode>();
        AU.add_post<ConstPro>();
    }
    bool always_invalid() const override final { return true; }

    void run(PassManager *mgr) override final;

  private:
    ir::BasicBlock *bb;
    ir::Instruction *inst;
    bool apply_rules();

    template <typename... Args>
    ir::IBinaryInst *insert_ibin(ir::IBinaryInst::IBinOp op, Args &&...args) {
        return bb->insert_inst<ir::IBinaryInst>(inst, op,
                                                std::forward<Args>(args)...);
    }
};

} // namespace pass
