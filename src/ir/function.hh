#pragma once

#include <list>

#include "global_variable.hh"
#include "value.hh"
#include "type.hh"
class Module;
class Argument;
class BasicBlock;

class Function:public Value, public ilist<Function>::node{
    public:
        Function()=default;
        ~Function()=default;
        static Function* create(FuncType* type, std::string& name, Module* parent);
        // Module
        Module* get_module() const { return _parent;}

        // Type
        Type* get_return_type() const;
        
        // Arguments
        unsigned get_num_of_args() const { return _args.size(); }

        const ilist<Argument>& get_args();
        
        void add_arg(Argument* arg);
        
        // BasicBlock
        unsigned get_num_basic_blcoks() const { return _bbs.size(); }
    
        BasicBlock& get_entry_block() { return _bbs.front(); }

        void add_basic_block(BasicBlock* bb);
        
        const ilist<BasicBlock>& get_basic_blocks() { return _bbs;}

        // print
        void set_instr_name();
        
    private:
        Function(FuncType* type, std::string& name, Module* parent);
        ilist<Argument> _args;
        ilist<BasicBlock> _bbs;
        Module* _parent;
        unsigned _seq_cnt;
};
class Argument: public Value, public ilist<Argument>::node{
    public:
        Argument()=default;
        Argument(Type* type, unsigned i, const std::string& name, Function* f)
            : Value(type, name), _arg_no(i), _parent(f) {}
        ~Argument()=default;
    private:
        unsigned _arg_no;
        Function* _parent;
};