#pragma once
#include "basic_block.hh"
#include "func_info.hh"
#include "global_variable.hh"
#include "instruction.hh"
#include "pass.hh"
#include "type.hh"
#include "utils.hh"
#include "value.hh"
#include <cassert>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <vector>

namespace pass {

class ArrayVisit final : public pass::TransformPass {
  public:
    ArrayVisit() = default;
    virtual void get_analysis_usage(pass::AnalysisUsage &AU) const override {
        using KillType = pass::AnalysisUsage::KillType;
        AU.set_kill_type(KillType::Normal);
        AU.add_require<FuncInfo>();
    }
    ~ArrayVisit() {
        for (auto mem : addrs) {
            delete mem;
        }
    }

    virtual void run(pass::PassManager *mgr) override;

    enum class AliasResult : uint8_t {
        NoAlias = 0,
        MayAlias,
        MustAlias,
    };

    static std::optional<int> get_const_int_val(ir::Value *v) {
        if (auto cv = dynamic_cast<ir::ConstInt *>(v))
            return cv->val();
        return std::nullopt;
    }

    class MemAddress {
      public:
        MemAddress(ir::Value *ptr) : _ptr(ptr) {
            _base = ptr; // MemAddress can only be GlobalVariable or GEP
            auto gep_inst = dynamic_cast<ir::GetElementPtrInst *>(ptr);
            // calculate the total const offset of GEP because a[0][4] equal to
            // a[1][0] for int a[2][4]
            // if any of the subscripts is variable, const_offset is nullopt
            const_offset = 0;
            while (gep_inst) {
                _base = gep_inst->get_operand(0);
                auto elem_type =
                    _base->get_type()->as<ir::PointerType>()->get_elem_type();

                for (unsigned idx = 1; idx < gep_inst->operands().size() &&
                                       const_offset.has_value();
                     idx++) {
                    auto off = get_const_int_val(gep_inst->get_operand(idx));
                    if (not off.has_value()) {
                        // there is a variable, then don't calculate the offset
                        const_offset = std::nullopt;
                        break;
                    }
                    if (elem_type->is_basic_type()) {
                        const_offset = const_offset.value() + off.value();
                        break;
                    }
                    auto array_type = elem_type->as<ir::ArrayType>();
                    const_offset = const_offset.value() +
                                   off.value() * array_type->get_total_cnt();
                    elem_type = array_type->get_elem_type();
                }
                gep_inst = dynamic_cast<ir::GetElementPtrInst *>(_base);
            }
        }
        MemAddress(MemAddress &other)
            : _ptr(other._ptr), _base(other._base),
              const_offset(other.const_offset) {}

        ir::Value *get_ptr() { return _ptr; }

        ir::Value *get_base() { return _base; }

        std::optional<unsigned> get_offset() { return const_offset; }

      private:
        ir::Value *_ptr;
        ir::Value *_base;
        std::optional<unsigned> const_offset;
    };

    // TODO:annotation
    AliasResult is_alias(MemAddress *lhs, MemAddress *rhs);

    void mem_visit(ir::BasicBlock *);

    MemAddress *alias_analysis(ir::Value *);

  private:
    std::map<ir::Value *, MemAddress *> ptr2addr;
    std::set<MemAddress *> addrs;
    std::map<MemAddress *, ir::Value *> latest_val{};
    std::set<ir::Instruction *> del_store_load;

    const FuncInfo::ResultType *_func_info;
};

}; // namespace pass
