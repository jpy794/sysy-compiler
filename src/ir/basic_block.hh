#pragma once
class BasicBlock: public Value{
    public:
        ~BasicBlock()=default;
        
        static BasicBlock* create(const std::string &name, Function* parent, Module* m);
        
        // BasicBlock
        const std::list<BasicBlock*>& get_pre_basic_blocks() { return pre_bbs_; }
        
        const std::list<BasicBlock*>& get_suc_basic_blocks() { return suc_bbs_; }
        
        void add_pre_basic_block(BasicBlock* bb) { pre_bbs_.push_back(bb); }
        
        void add_suc_basic_block(BasicBlock* bb) { suc_bbs_.push_back(bb);}
        
        unsigned get_num_pre_bbs() const { return pre_bbs_.size(); }

        unsigned get_num_suc_bbs() const { return suc_bbs_.size(); }
        // Instruction
        void add_instruction(Instruction* instr);

        unsigned get_num_of_instr() const { return instr_list_.size();}

        //TODO:print
    private:
        BasicBlock(const std::string &name, Function* parent);
        std::list<BasicBlock*> pre_bbs_;
        std::list<BasicBlock*> suc_bbs_;
        std::list<Instruction*> instr_list_;
        Function* parent_;
};