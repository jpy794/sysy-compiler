#pragma once
#include <string>
#include <vector>
#include "value.hh"
class User: public Value{
    public:
        User()=default;
        User(Type* type, std::vector<Value*>& operands, const std::string& name="")
            : Value(type, name), _operands(operands) {}
        
        ~User()=default;
        
        unsigned get_num_of_operands() const { return _operands.size(); }

        Value* get_operand(unsigned i) const { return _operands[i]; }

        std::vector<Value*>& get_operands() { return _operands; }

        void set_operand(Value* val, unsigned i) { _operands[i] = val; }

    private:
        std::vector<Value*> _operands;
};