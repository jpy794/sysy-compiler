#pragma once

namespace mir {

enum class Stage { stage1, stage2 };
enum class Role { Full, NameOnly };

struct MIRContext {
    const Stage stage;
    Role role;
    explicit MIRContext(Stage s, Role r) : stage(s), role(r) {}
    explicit MIRContext(const MIRContext &c) : stage(c.stage), role(c.role) {}
};

} // namespace mir
