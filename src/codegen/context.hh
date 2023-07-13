#pragma once

#include "mir_function.hh"
#include "regalloc.hh"

namespace context {

enum class Stage { stage1, stage2 };
enum class Role { Full, NameOnly };

struct Context {
    static const char *TAB;
    Stage stage;
    Role role;
    unsigned indent_level{0};
    const codegen::RegAlloc &allocator;

    const mir::Function *cur_function{nullptr};

    // constructors
    explicit Context(Stage s, Role r, const codegen::RegAlloc &alloc)
        : stage(s), role(r), allocator(alloc) {}
    explicit Context(const Context &c)
        : stage(c.stage), role(c.role), allocator(c.allocator),
          cur_function(c.cur_function) {}
    Context(const Context &&c)
        : stage(c.stage), role(c.role), allocator(c.allocator),
          cur_function(c.cur_function) {}

    Context name_only() const {
        Context name_only_context{stage, Role::NameOnly, allocator};
        return name_only_context;
    }
    Context indent() const {
        Context indented_context{*this};
        indented_context.indent_level = indent_level + 1;
        return indented_context;
    }
};

inline const char *Context::TAB = "  ";

// struct : public mir::MIRContext {};
}; // namespace context
