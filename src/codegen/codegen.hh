#pragma once

#include "context.hh"
#include "instruction.hh"
#include "mir_builder.hh"
#include "mir_module.hh"
#include "module.hh"
#include "regalloc.hh"

#include <bitset>
#include <cassert>
#include <iostream>
#include <memory>
#include <ostream>
#include <string>

namespace codegen {

class CodeGen {

    context::Stage _stage{context::Stage::stage1};
    std::unique_ptr<mir::Module> _mir_module{nullptr};

    RegAlloc _allocator;

  public:
    CodeGen(std::unique_ptr<ir::Module> &&ir_module) {
        mir::MIRBuilder builder(std::move(ir_module));
        _mir_module.reset(builder.release());
        _allocator.run(_mir_module.get());
    }

    friend std::ostream &operator<<(std::ostream &os, const CodeGen &c);

  private:
    void construct_mir(ir::Module *);
};

}; // namespace codegen
