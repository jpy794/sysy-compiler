#pragma once

#include "mir_function.hh"
#include "regalloc.hh"

namespace context {

enum class Stage { stage1, stage2 };
enum class Role { Full, NameOnly };

struct Context {
    Stage stage;
    Role role;
    const codegen::RegAlloc &allocator;

    const mir::Function *cur_function{nullptr};

    explicit Context(Stage s, Role r, const codegen::RegAlloc &alloc)
        : stage(s), role(r), allocator(alloc) {}
    explicit Context(const Context &c)
        : stage(c.stage), role(c.role), allocator(c.allocator) {}
    Context(const Context &&c)
        : stage(c.stage), role(c.role), allocator(c.allocator) {}

    Context name_only() const {
        Context name_only_context{stage, Role::NameOnly, allocator};
        ;
        return name_only_context;
    }
};

// struct : public mir::MIRContext {};
}; // namespace context
