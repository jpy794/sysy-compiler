#pragma once

#include "mir_function.hh"
#include "mir_module.hh"

#include <limits>
#include <map>
#include <set>
#include <vector>

namespace codegen {

// liveness info
using LiveVarSet = std::set<mir::Register::RegIDType>;
using LivenessInfo = std::vector<LiveVarSet>;
// control flow info
using LabelOrder = std::vector<const mir::Label *>;
using ProgramPoint = unsigned;
using InstructionID = ProgramPoint;
using InstIDMap = std::map<const mir::Instruction *, InstructionID>;

inline ProgramPoint IN_POINT(InstructionID id) { return 2 * id; }
inline ProgramPoint OUT_POINT(InstructionID id) { return 2 * id + 1; }

struct ControlFlowInfo {
    const mir::Function *func;
    LabelOrder label_order;
    InstIDMap instid;

    explicit ControlFlowInfo() = default;
    explicit ControlFlowInfo(const mir::Function *func) { run(func); }
    ControlFlowInfo(const ControlFlowInfo &) = delete;
    ControlFlowInfo(ControlFlowInfo &&other) = default;

    void clear() {
        func = nullptr;
        label_order.clear();
        instid.clear();
    }

    void run(const mir::Function *func) {
        clear();
        this->func = func;
        get_dfs_order(func);
        get_inst_id();
    }

  private:
    void get_dfs_order(const mir::Function *);
    void get_inst_id();
};

struct LivenessAnalysis {
    LivenessInfo live_info;

    LivenessAnalysis() = default;
    LivenessAnalysis(const ControlFlowInfo &cfg, bool want_float, bool pesudo) {
        run(cfg, want_float, pesudo);
    }
    LivenessAnalysis(const LivenessAnalysis &) = delete;
    LivenessAnalysis(LivenessAnalysis &&) = default;

    void run(const ControlFlowInfo &cfg_info, bool want_float, bool pesudo);

    void clear() { live_info.clear(); }
    void reset(size_t inst_count) {
        clear();
        live_info.resize(2 * inst_count);
    }
};

struct LiveInterVal {
    const mir::IPReg::RegIDType vreg_id;
    ProgramPoint start{std::numeric_limits<ProgramPoint>::max()}, end{0};

    LiveInterVal(mir::IPReg::RegIDType id) : vreg_id(id) {}
    LiveInterVal(mir::IPReg::RegIDType id, ProgramPoint l, ProgramPoint r)
        : vreg_id(id), start(l), end(r) {}
    void update(ProgramPoint p) {
        start = std::min(start, p);
        end = std::max(end, p);
    }
    bool check() { // sanity check only for now
        if (start < end)
            return true;
        if (start == end) {
            assert(start == 0);
            return true; // valid case: arg used only once at start
        }
        throw unreachable_error{};
    }

    struct IncreasingStartPoint {
        bool operator()(const LiveInterVal &l1, const LiveInterVal &l2) const {
            if (l1.start != l2.start)
                return l1.start < l2.start;
            return &l1 < &l2;
        }
    };
    struct IncreasingEndPoint {
        bool operator()(const LiveInterVal &l1, const LiveInterVal &l2) const {
            if (l1.end != l2.end)
                return l1.end < l2.end;
            return &l1 < &l2;
        }
    };
};
} // namespace codegen
