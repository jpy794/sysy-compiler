#pragma once

#include "instruction.hh"
#include "mir_builder.hh"
#include "mir_context.hh"
#include "mir_module.hh"
#include "module.hh"

#include <bitset>
#include <cassert>
#include <iostream>
#include <memory>
#include <ostream>
#include <string>

namespace codegen {

class CodeGen {

    // std::unique_ptr<mir::Module> _mir_moduler;
    mir::MIRBuilder _builder;
    mir::Stage _stage{mir::Stage::stage1};
    std::unique_ptr<mir::Module> mir_module{nullptr};

  public:
    CodeGen(std::unique_ptr<ir::Module> &&ir_module)
        : _builder(std::move(ir_module)) {
        mir_module.reset(_builder.release());
    }
    friend std::ostream &operator<<(std::ostream &os, const CodeGen &c);

  private:
    void construct_mir(ir::Module *);
};

}; // namespace codegen
