#include "regalloc.hh"

using namespace codegen;

void LinearScanImpl::run() {
    // _active.clear();
    for (auto &interval : _intervals) {
        expire_old_intervals(interval);
        if (_pool.empty())
            spill_at(interval);
        else {
            _map.insert({interval.vreg_id, _pool.top()});
            _pool.pop();
            _active.insert(interval);
        }
    }
}

void LinearScanImpl::expire_old_intervals(const LiveInterVal &ref) {
    for (auto iter = _active.begin(); iter != _active.end();) {
        if (iter->end >= ref.start)
            return;
        auto expired = iter++;
        _pool.push(_map.at(expired->vreg_id));
        _active.erase(expired);
    }
}

void LinearScanImpl::spill_at(const LiveInterVal &interval) {
    auto spill = _active.rbegin();
    if (spill->end > interval.end) {
        auto iter = _map.find(spill->vreg_id);
        assert(iter != _map.end());
        auto physical_reg = iter->second;
        // spill out
        _spilled.insert(spill->vreg_id);
        _map.erase(iter);
        // erase with reverse_iterator: https://stackoverflow.com/q/1830158
        _active.erase(std::next(spill).base());
        // insert new reg map
        _map.insert({interval.vreg_id, physical_reg});
        _active.insert(interval);
    } else {
        _spilled.insert(interval.vreg_id);
    }
}
