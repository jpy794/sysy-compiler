#pragma once

#include "err.hh"
#include "mir_module.hh"
#include "mir_register.hh"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <unordered_set>

namespace codegen {

using ConstIPRegPtr = mir::PhysicalRegisterManager::ConstIPRegPtr;

// secondary packaging, add priority info
struct RegInfo {
    unsigned priority; // the smaller, the better
    ConstIPRegPtr reg;

    RegInfo(unsigned p, ConstIPRegPtr r) : priority(p), reg(r) {}
    RegInfo(const RegInfo &info) : priority(info.priority), reg(info.reg) {}
    void operator=(const RegInfo &other) {
        priority = other.priority;
        reg = other.reg;
    }

    // if priority level is smaller than the other
    bool operator<(const RegInfo &other) const {
        if (priority != other.priority)
            return priority > other.priority;
        return reg->get_id() > other.reg->get_id();
    }
};

// liveness info
using LiveVarSet = std::set<mir::Register::RegIDType>;
using LivenessInfo = std::vector<LiveVarSet>;
// control flow info
using LabelOrder = std::vector<const mir::Label *>;
using ProgramPoint = unsigned;
using InstructionID = ProgramPoint;
using InstIDMap = std::map<const mir::Instruction *, InstructionID>;
// register info
using RegisterPool = std::priority_queue<RegInfo>;
using RegisterMap = std::map<mir::Register::RegIDType, RegInfo>;
using SpilledSet = std::set<mir::Register::RegIDType>;
// allocation result, all result is function level
template <class T> using FuncResultMap = std::map<const mir::Function *, T>;

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

    LivenessAnalysis(const ControlFlowInfo &cfg_info, bool want_float);
    LivenessAnalysis(const LivenessAnalysis &) = delete;
    LivenessAnalysis(LivenessAnalysis &&) = default;

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

// TODO add pre allocated map
// used on function, and 1 instance can only parse int/float
class RegAllocImpl {
  protected:
    RegisterPool _pool;  // initial free register pool
    RegisterMap _map;    // result: virtual reg to real reg
    SpilledSet _spilled; // result: stack allocation

  public:
    explicit RegAllocImpl(const RegisterPool &pool) : _pool(pool) {}
    virtual ~RegAllocImpl() {}

    virtual void run() = 0;

    const RegisterMap &get_reg_map() const { return _map; }
    const SpilledSet &get_spilled() const { return _spilled; }
};

class LinearScanImpl : public RegAllocImpl {
  public:
    using LiveInts = std::set<LiveInterVal, LiveInterVal::IncreasingStartPoint>;
    using ActiveSet = std::set<LiveInterVal, LiveInterVal::IncreasingEndPoint>;

  private:
    const LiveInts _intervals;
    ActiveSet _active;

  public:
    LinearScanImpl(const RegisterPool &pool, LiveInts &&ints)
        : RegAllocImpl(pool), _intervals(ints) {}
    void run() override final;
    const LiveInts &get_live_ints() const { return _intervals; }

  private:
    void expire_old_intervals(const LiveInterVal &ref);
    void spill_at(const LiveInterVal &interval);
};

class RegAlloc {
    // register pool with priority
    RegisterPool _reg_pool_int, _reg_pool_float;
    // control flow info
    FuncResultMap<ControlFlowInfo> _cfg_info;
    // liveness analysis info
    FuncResultMap<LivenessAnalysis> _liveness_int, _liveness_float;
    // register allocation info
    FuncResultMap<std::unique_ptr<RegAllocImpl>> _impl_int, _impl_float;

  public:
    RegAlloc();

    void run(const mir::Module *m);

    void clear() {
        _cfg_info.clear();
        _liveness_int.clear();
        _liveness_float.clear();
    }

    const ControlFlowInfo &get_cfg_info(const mir::Function *func) const {
        return _cfg_info.at(func);
    }
    const LivenessInfo &get_liveness(const mir::Function *func,
                                     bool want_float) const {
        return (want_float ? _liveness_float.at(func) : _liveness_int.at(func))
            .live_info;
    }
    const RegisterMap &get_reg_map(const mir::Function *func,
                                   bool want_float) const {
        return want_float ? _impl_float.at(func)->get_reg_map()
                          : _impl_int.at(func)->get_reg_map();
    }
    const SpilledSet &get_spilled(const mir::Function *func,
                                  bool want_float) const {
        return want_float ? _impl_float.at(func)->get_spilled()
                          : _impl_int.at(func)->get_spilled();
    }

    const LinearScanImpl::LiveInts &get_live_ints(const mir::Function *func,
                                                  bool want_float) const {
        auto ptr =
            (want_float ? _impl_float.at(func) : _impl_int.at(func)).get();
        assert(is_a<LinearScanImpl>(ptr));
        return as_a<LinearScanImpl>(ptr)->get_live_ints();
    }

  private:
    LinearScanImpl::LiveInts make_interval(mir::Function *func, bool for_float);
};

} // namespace codegen
