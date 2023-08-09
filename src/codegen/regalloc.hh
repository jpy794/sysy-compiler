#pragma once

#include "err.hh"
#include "liveness.hh"
#include "mir_module.hh"
#include "mir_register.hh"

#include <map>
#include <queue>
#include <set>

namespace codegen {

using ConstRegPtr = mir::PhysicalRegisterManager::ConstRegPtr;

// secondary packaging, add priority info
struct RegInfo {
    unsigned priority; // the smaller, the better
    ConstRegPtr reg;

    RegInfo(unsigned p, ConstRegPtr r) : priority(p), reg(r) {}
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

// register info
using RegisterPool = std::priority_queue<RegInfo>;
using RegisterMap = std::map<mir::Register::RegIDType, RegInfo>;
using SpilledSet = std::set<mir::Register::RegIDType>;
// allocation result, all result is function level
template <class T> using FuncResultMap = std::map<const mir::Function *, T>;

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
