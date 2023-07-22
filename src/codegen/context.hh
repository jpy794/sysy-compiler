#pragma once

#include "mir_function.hh"
#include "regalloc.hh"

namespace context {

enum class Stage { stage1, stage2 };
enum class Role { Full, NameOnly };

struct Context {
    static const char *TAB;

    unsigned indent_level{0};

    Stage stage;
    Role role;
    const codegen::RegAlloc &allocator;
    bool output_comment{true};

    const mir::Function *cur_function{nullptr};

    // constructors
    explicit Context(Stage s, Role r, const codegen::RegAlloc &alloc,
                     bool comment)
        : stage(s), role(r), allocator(alloc), output_comment(comment) {}
    explicit Context(const Context &c)
        : stage(c.stage), role(c.role), allocator(c.allocator),
          output_comment(c.output_comment), cur_function(c.cur_function) {}
    Context(const Context &&c)
        : stage(c.stage), role(c.role), allocator(c.allocator),
          output_comment(c.output_comment), cur_function(c.cur_function) {}

    Context name_only() const {
        Context name_only_context{*this};
        name_only_context.role = Role::NameOnly;
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
