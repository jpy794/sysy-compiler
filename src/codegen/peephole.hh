#include "liveness.hh"
#include "mir_function.hh"
#include "mir_instruction.hh"
#include "mir_label.hh"
#include "mir_module.hh"
#include "mir_register.hh"
#include "utils.hh"

#include <array>
#include <deque>
#include <functional>
#include <map>
#include <variant>
#include <vector>

namespace codegen {

class PeepholeOpt {
    static constexpr int PEEPHOLE_SIZE = 4;

    using Peephole = std::deque<mir::Instruction *>;

    // Return value meaning: <changed, broken>
    // @changed: if the some change happens
    // @broken: the pass has add/delete inst, make peephole broken.
    using PassRet = std::pair<bool, bool>;
    using Pass = std::function<PassRet()>;

    mir::Module &_module;
    struct {
        mir::Function *cur_function{nullptr};
        mir::Label *cur_label{nullptr};
        /* ControlFlowInfo cfg_info{};
         * LivenessAnalysis liveness_info{}; */
    } _context;

    Peephole _peephole;

  public:
    PeepholeOpt(mir::Module &mod) : _module(mod) {}
    void run();

  private:
    PassRet combine_load_store_const_off();
    /* li r1 4/8/16/...
     * mul r2, r2, r1 ==> slli r2, r2, 2 */
    PassRet mul2shift();
    /* li r1 imm
     * subw r2, r3, r1 ==> addiw r2, r3, imm */
    PassRet subw2addiw();
    /* mv r1 r2
     * op r3 r1 r4 ==> op r3 r2 r4
     *             ==> mv r3 ..(if r2/r4 is x0) */
    PassRet naive_coalesce();

    // mir level DCE
    bool remove_useless_inst();
};

} // namespace codegen
