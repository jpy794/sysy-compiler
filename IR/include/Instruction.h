#ifndef IR_INSTRUCTION
#define IR_INSTRUCTION
class Instruction: public Value{
    public:
        enum OpID{
        // Terminator Instructions
        ret,
        br,
        // Standard binary operators
        add,
        sub,
        mul,
        sdiv,
        // float binary operators
        fadd,
        fsub,
        fmul,
        fdiv,
        // Memory operators
        alloca,
        load,
        store,
        // Other operators
        cmp,
        fcmp,
        phi,
        call,
        getelementptr,
        fptosi,
        sitofp
        };
        Instruction(Type* type, OpID id, BasicBlock* parent);
        ~Instruction=defalut;
    private:
        std::vector<Value*> operands_;
        OpID id_;
        BasicBlock* parent_;
};
#endif