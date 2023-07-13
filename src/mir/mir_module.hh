#pragma once

#include "err.hh"
#include "mir_config.hh"
#include "mir_function.hh"
#include "mir_memory.hh"

#include <memory>
#include <string>
#include <vector>

namespace mir {
class Module {
    friend class CodeGen;
    std::vector<Function *> _functions;
    std::vector<GlobalObject *> _globals;

  public:
    Module() = default;

    void dump(std::ostream &os, const Context &context) const;
    const std::vector<Function *> &get_functions() const { return _functions; }

    template <typename... Args> Function *add_function(Args... args) {
        _functions.push_back(ValueManager::get().create<Function>(args...));
        return _functions.back();
    }

    template <typename... Args> GlobalObject *add_global(Args... args) {
        _globals.push_back(ValueManager::get().create<GlobalObject>(args...));
        return _globals.back();
    }
};
}; // namespace mir
