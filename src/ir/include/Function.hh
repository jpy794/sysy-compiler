#ifndef IR_FUNCTION
#define IR_FUNCTION
class Function:public Value{
    public:
        ~Function();
        static Function* create(FuncType* type, std::string& name, Module* parent);
        // Type
        Type* get_return_type() const;
        
        // Arguments
        unsigned get_num_of_args() const { return args_.size(); }

        const list<Argument*>& get_args();
        
        void add_arg(Argument* arg);
        
        // BasicBlock
        unsigned get_num_basic_blcoks() const { return bbs_.size(); }
    
        BasicBlock* get_entry_block() const { return bbs_.front(); }

        void add_basic_block(BasicBlock* bb);
        
        const std::list<BasicBlock*>& get_basic_blocks() { return bbs_;}

        // TODO:print
        void set_instr_name();
        
    private:
        Function(FuncType* type, std::string& name, Module* parent);
        std::list<Argument*> args_;
        std::list<BasicBlock*> bbs_;//attention!Maybe bb will replace its func it belongs to in future work
        Module* parent_;
        unsigned seq_cnt_;
};
class Argument: public Value {
    public:
        Argument(Type* type, unsigned i, const string& name, Function* f);
        ~Argument()=default;
    private:
        Function* parent_;
        unsigned arg_no_;
}
#endif