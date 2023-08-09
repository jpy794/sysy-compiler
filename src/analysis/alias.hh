#pragma once
#include "constant.hh"
#include "function.hh"
#include "global_variable.hh"
#include "instruction.hh"
#include "pass.hh"
#include "utils.hh"
#include "value.hh"
#include <optional>

namespace pass {
class AliasAnalysis final : public pass::AnalysisPass {
  public:
    explicit AliasAnalysis() {}
    ~AliasAnalysis() = default;

    enum class AliasResult : uint8_t {
        NoAlias = 0,
        MayAlias,
        MustAlias,
    };

    struct ResultType {
        AliasResult alias(ir::Value *v1, ir::Value *v2) {
            auto v1_addr = MemAddress(v1);
            auto v2_addr = MemAddress(v2);
            if (v1_addr.has_precise_base() && v2_addr.has_precise_base()) {
                if (v1_addr == v2_addr)
                    return AliasResult::MustAlias;
                if (v1_addr != v2_addr)
                    return AliasResult::NoAlias;
            }
            // for arguments, caller can pass the same address as different
            // parameters
            return AliasResult::MayAlias;
        };
    };

    virtual void get_analysis_usage(pass::AnalysisUsage &AU) const override {
        using KillType = pass::AnalysisUsage::KillType;
        AU.set_kill_type(KillType::None);
    }

    virtual std::any get_result() const override { return &_result; }

    virtual void run(pass::PassManager *mgr) override;

    virtual void clear() override;

    static std::optional<int> get_const_int_val(ir::Value *v) {
        if (auto cv = dynamic_cast<ir::ConstInt *>(v))
            return cv->val();
        return std::nullopt;
    }

    class MemAddress {
      public:
        MemAddress(ir::Value *ptr) : _ptr(ptr) {
            std::vector<std::optional<ir::Value *>> offset_reverse;
            _base = ptr; // MemAddress can only be GlobalVariable or GEP
            auto gep_inst = dynamic_cast<ir::GetElementPtrInst *>(ptr);
            while (gep_inst) {
                unsigned size = gep_inst->operands().size();
                ir::Value *last_off = gep_inst->get_operand(size - 1);
                if (not offset_reverse
                            .empty()) { // It indicates that the previous
                                        // gep_inst is offset based on the
                                        // results of new gep_inst.Thus,
                                        // their partial offsets need to be
                                        // merged
                    if (offset_reverse.back().has_value()) {
                        ir::Value *back_val = offset_reverse.back().value();
                        // for a new 0 offset, the newest offset will be set
                        // as other offset
                        if (is_a<ir::ConstInt>(back_val) &&
                            as_a<ir::ConstInt>(back_val)->val() == 0) {
                            offset_reverse.back() = last_off;
                        } else if (is_a<ir::ConstInt>(last_off) &&
                                   as_a<ir::ConstInt>(last_off)->val() == 0) {
                            // do nothing
                        } else if (is_a<ir::ConstInt>(last_off) &&
                                   is_a<ir::ConstInt>(
                                       back_val)) { // if they are both
                                                    // constant, then offset
                                                    // can be calculated
                            offset_reverse.back() =
                                ir::Constants::get().int_const(
                                    as_a<ir::ConstInt>(back_val)->val() +
                                    as_a<ir::ConstInt>(last_off)->val());
                        } else { // for other cases, offset is unknown
                            offset_reverse.back() = std::nullopt;
                        }
                    }
                } else {
                    offset_reverse.push_back(last_off);
                }
                for (unsigned i = size - 2; i >= 1; i--) {
                    offset_reverse.push_back(gep_inst->get_operand(i));
                }
                _base = gep_inst->get_operand(0);
                gep_inst = dynamic_cast<ir::GetElementPtrInst *>(_base);
            }
            offset.assign(offset_reverse.rbegin(), offset_reverse.rend());
        }

        ir::Value *get_base() { return _base; }

        bool operator==(const MemAddress &other) {
            if (_ptr == other._ptr)
                return true;
            if (this->offset.size() != other.offset.size())
                return false;
            for (unsigned i = 0; i < offset.size(); i++) {
                if (offset[i].has_value() && other.offset[i].has_value()) {
                    if (offset[i].value() != other.offset[i].value())
                        return false;
                } else
                    return false;
            }
            return true;
        }
        bool operator!=(const MemAddress &other) { return not(*this == other); }

        bool has_precise_base() {
            return is_a<ir::AllocaInst>(_base) ||
                   is_a<ir::GlobalVariable>(_base);
        }

      private:
        ir::Value *_ptr;
        ir::Value *_base;
        std::vector<std::optional<ir::Value *>> offset;
    };

  private:
    ResultType _result;
};
} // namespace pass