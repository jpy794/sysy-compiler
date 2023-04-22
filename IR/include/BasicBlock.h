#ifndef IR_BASICBLOCK
#define IR_BASICBLOCK
class BasicBlock: public Value{
    public:
        BasicBlock(Module* m, Function* parent, const std::string &name);
        ~BasicBlock=defalut;
    private:
        std::list<BasicBlock*> pre_bbs_;
        std::list<BasicBlock*> suc_bbs_;
        std::list<Instruction*> instr_list_;
        Function* parent_;
};
#endif