#include "peephole.hh"
#include "liveness.hh"
#include "mir_immediate.hh"
#include "mir_instruction.hh"
#include "mir_register.hh"
#include "mir_value.hh"

#include <cmath>

using namespace std;
using namespace mir;
using namespace codegen;

static auto sp = PhysicalRegisterManager::get().sp();
static auto zero = PhysicalRegisterManager::get().zero();

Immediate *create_imm12(int imm) {
    return ValueManager::get().create<Imm12bit>(imm);
}

inline bool is_power_of_2(int x) { return x and !(x & (x - 1)); }

// @return: <contains, the other src(if exists)>
pair<bool, vector<Value *>> inst_src_contains(Instruction *inst, Value *v) {
    bool contains;
    vector<Value *> other_src;

    for (auto i = inst->will_write_register() ? 1 : 0;
         i < inst->get_operand_num(); ++i) {
        auto op = inst->get_operand(i);
        if (op == v)
            contains = true;
        else
            other_src.push_back(op);
    }

    return {contains, other_src};
}

void PeepholeOpt::run() {
    const std::array _passes{
        std::bind(&PeepholeOpt::mul2shift, this),
        std::bind(&PeepholeOpt::subw2addiw, this),
        std::bind(&PeepholeOpt::naive_coalesce, this),
    };

    for (auto func : _module.get_functions()) {
        if (not func->is_definition())
            continue;
        _context.cur_function = func;

        do {
            for (auto label : func->get_labels()) {
                _context.cur_label = label;
                auto &insts = label->get_insts();

                bool cont = true;
                while (cont) {
                    cont = false;
                    for (auto pass : _passes) {
                        _peephole.clear();
                        auto run_pass = [&]() {
                            auto [changed, broken] = pass();
                            if (broken)
                                _peephole.clear();
                            cont |= changed;
                        };
                        for (auto inst_iter = insts.begin();
                             inst_iter != insts.end();) {
                            // be careful about safe traverse
                            auto inst = &*inst_iter;
                            ++inst_iter;
                            if (inst_iter->get_opcode() == mir::COMMENT)
                                continue;
                            // maintain peephole
                            _peephole.push_back(inst);
                            if (_peephole.size() > PEEPHOLE_SIZE)
                                _peephole.pop_front();
                            run_pass();
                        }
                        while (_peephole.size()) {
                            _peephole.pop_front();
                            run_pass();
                        }
                    }
                }
            }
        } while (
            remove_useless_inst()); // This relys on function level liveness
                                    // analysis. To avoid unafordable overhead,
                                    // run at the end only for each function
    }
}

PeepholeOpt::PassRet PeepholeOpt::mul2shift() {
    if (_peephole.size() < 2)
        return {false, false};
    auto is_mul_inst = [](MIR_INST opcode) {
        return opcode == MUL or opcode == MULW;
    };
    auto inst0 = _peephole.at(0);
    auto inst1 = _peephole.at(1);
    if (not(inst0->get_opcode() == LoadImmediate and
            is_mul_inst(inst1->get_opcode())))
        return {false, false};
    auto li_dest = inst0->get_operand(0);
    auto li_src = inst0->get_operand(1);
    if (not(li_dest == inst1->get_operand(2)))
        return {false, false};
    if (not(is_a<Imm32bit>(li_src) and
            is_power_of_2(as_a<Imm32bit>(li_src)->get_imm())))
        return {false, false};

    int slli_bit = log2(as_a<Imm32bit>(li_src)->get_imm());
    inst1->change_opcode(SLLI);
    inst1->set_operand(2, create_imm12(slli_bit));
    return {true, false};
}

PeepholeOpt::PassRet PeepholeOpt::naive_coalesce() {
    if (_peephole.size() < 2)
        return {false, false};
    auto inst0 = _peephole.at(0);
    auto inst1 = _peephole.at(1);

    auto effective_range = [](MIR_INST opcode) { return opcode <= REMW; };

    if (inst0->get_opcode() != Move or not effective_range(inst1->get_opcode()))
        return {false, false};
    auto reg_dest = inst0->get_operand(0);
    auto reg_src = inst0->get_operand(1);

    PassRet ret{false, false};

    // substitute
    for (auto i = 1; i < inst1->get_operand_num(); ++i) {
        if (inst1->get_operand(i) == reg_dest) {
            inst1->set_operand(i, reg_src);
            ret.first = true;
        }
    }
    // further: use mv if possible
    bool qualified{false};
    Value *src{nullptr};

    // parse validity and value-src
    switch (inst1->get_opcode()) {
    case ADD:
    case ADDW: {
        auto [contains, the_other_src] = inst_src_contains(inst1, zero);
        qualified = contains;
        src = the_other_src.at(0); // assert no add zero zero
        break;
    }
    case ADDI:
    case ADDIW:
        qualified = as_a<Imm12bit>(inst1->get_operand(2))->get_imm() == 0;
        src = inst1->get_operand(1);
        break;
    default:
        break;
    }
    // substitute to move inst
    if (qualified) {
        _context.cur_label->insert_before(inst1, Move,
                                          {inst1->get_operand(0), src});
        _context.cur_label->get_insts().erase(inst1);
        ret.second = true;
    }
    return ret;
}

PeepholeOpt::PassRet PeepholeOpt::subw2addiw() {
    if (_peephole.size() < 2)
        return {false, false};
    auto inst0 = _peephole.at(0);
    auto inst1 = _peephole.at(1);
    if (inst0->get_opcode() != LoadImmediate or inst1->get_opcode() != SUBW)
        return {false, false};

    bool changed = false;
    int imm = as_a<Immediate>(inst0->get_operand(1))->get_imm();
    auto li_dest = inst0->get_operand(0);

    if (li_dest == inst1->get_operand(2) and Imm12bit::check_in_range(-imm)) {
        inst1->change_opcode(ADDIW);
        inst1->set_operand(2, create_imm12(-imm));
        changed = true;
    }

    return {changed, false};
}

bool PeepholeOpt::remove_useless_inst() {
    auto func = _context.cur_function;
    ControlFlowInfo cfg_info(func);
    LivenessAnalysis live_info(cfg_info, false, true);

    bool changed = false;

    for (auto label : func->get_labels()) {
        if (label->get_type() != Label::LabelType::Normal)
            continue;

        for (auto &inst : label->get_insts()) {
            if (not inst.will_write_register())
                continue;
            auto dest_reg = as_a<PhysicalRegister>(inst.get_operand(0));
            if (dest_reg->is_float_register() or dest_reg == sp)
                continue;
            auto instid = cfg_info.instid.at(&inst);
            auto outset = live_info.live_info.at(OUT_POINT(instid));
            if (not contains(outset, dest_reg->get_id())) {
                changed = true;
                label->get_insts().erase(&inst);
            }
        }
    }
    return changed;
}
