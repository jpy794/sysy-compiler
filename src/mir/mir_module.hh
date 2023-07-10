#pragma once

#include "err.hh"
#include "mir_config.hh"
#include "mir_context.hh"
#include "mir_function.hh"
#include "mir_memory.hh"

#include <antlr4-runtime/ConsoleErrorListener.h>
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

    void dump(std::ostream &os, Stage stage) const {
        MIRContext context{stage, Role::Full};
        for (auto func : _functions) {
            if (not func->is_definition())
                continue;
            func->dump(os, context);
        }
        // output global at the end
        for (auto global : _globals) {
            global->dump(os, context);
        }
    }

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
