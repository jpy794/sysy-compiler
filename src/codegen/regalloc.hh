#pragma once

#include "mir_module.hh"

namespace codegen {

// liveness info
using LiveVarSet = std::unordered_set<mir::Register::RegIDType>;
using LivenessInfo = std::vector<LiveVarSet>;
// control flow info
using LabelOrder = std::vector<const mir::Label *>;
using ProgramPoint = unsigned;
using InstructionID = ProgramPoint;
using InstIDMap = std::map<const mir::Instruction *, InstructionID>;
// register info
using RegisterMap = std::map<mir::Register::RegIDType, mir::PhysicalRegister *>;

inline ProgramPoint IN_POINT(InstructionID id) { return 2 * id; }
inline ProgramPoint OUT_POINT(InstructionID id) { return 2 * id + 1; }

struct ControlFlowInfo {
    LabelOrder label_order;
    InstIDMap instid;

    explicit ControlFlowInfo(const mir::Function *func) {
        get_dfs_order(func);
        get_inst_id();
    }
    ControlFlowInfo(const ControlFlowInfo &) = delete;
    ControlFlowInfo(ControlFlowInfo &&other) = default;

  private:
    void get_dfs_order(const mir::Function *);
    void get_inst_id();
};

struct LivenessAnalysis {
    LivenessInfo live_info;

    LivenessAnalysis(const ControlFlowInfo &cfg_info, bool integer);
    LivenessAnalysis(const LivenessAnalysis &) = delete;
    LivenessAnalysis(LivenessAnalysis &&) = default;

    void clear() { live_info.clear(); }
    void reset(size_t inst_count) {
        clear();
        live_info.resize(2 * inst_count);
    }
};

class RegAlloc {
    std::map<const mir::Function *, ControlFlowInfo> _cfg_info;
    std::map<const mir::Function *, LivenessAnalysis> _liveness_int;
    std::map<const mir::Function *, LivenessAnalysis> _liveness_float;
    // std::map<mir::Function *, RegisterMap> _reg_alloc_info;

  public:
    RegAlloc() = default;

    void run(const mir::Module *m) {
        for (auto func : m->get_functions()) {
            if (not func->is_definition())
                continue;
            _cfg_info.insert({func, ControlFlowInfo(func)});
            _liveness_int.insert(
                {func, LivenessAnalysis(_cfg_info.at(func), true)});
            _liveness_float.insert(
                {func, LivenessAnalysis(_cfg_info.at(func), false)});
        }
    }

    const ControlFlowInfo &get_cfg_info(const mir::Function *func) const {
        return _cfg_info.at(func);
    }
    const LivenessAnalysis &get_liveness_int(const mir::Function *func) const {
        return _liveness_int.at(func);
    }
    const LivenessAnalysis &
    get_liveness_float(const mir::Function *func) const {
        return _liveness_float.at(func);
    }

  private:
};

} // namespace codegen
