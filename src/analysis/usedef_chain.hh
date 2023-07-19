#pragma once
#include "instruction.hh"
#include "module.hh"
#include "pass.hh"
#include "user.hh"
#include "value.hh"
#include <functional>
#include <list>
#include <map>

namespace pass {
class UseDefChain final : public pass::AnalysisPass {
  public:
    explicit UseDefChain() {}
    ~UseDefChain() = default;

    struct ResultType {
        struct Use {
            Use(ir::User *user, unsigned idx) : user(user), op_idx(idx) {}
            ir::User *user;
            unsigned op_idx;
        };
        std::map<ir::Value *, std::list<Use>> users;
        void replace_all_use_with(ir::Value *old_val,
                                  ir::Value *new_val) const {
            for (auto use : users.at(old_val)) {
                use.user->set_operand(use.op_idx, new_val);
            }
        }
        void replace_use_when(ir::Value *old_val, ir::Value *new_val,
                              std::function<bool(ir::User *)> pred) const {
            auto &_users = users.at(old_val);
            for (auto it = _users.begin(); it != _users.end();) {
                auto use = *it;
                auto user = use.user;
                ++it;
                if (not pred(user)) {
                    continue;
                }
                user->set_operand(use.op_idx, new_val);
            }
        }
    };

    virtual void get_analysis_usage(pass::AnalysisUsage &AU) const override {
        using KillType = pass::AnalysisUsage::KillType;
        AU.set_kill_type(KillType::None);
    }

    virtual std::any get_result() const override { return &_result; }

    virtual void run(pass::PassManager *mgr) override;

    virtual void clear() override { _result.users.clear(); }

  private:
    ResultType _result;
};
} // namespace pass