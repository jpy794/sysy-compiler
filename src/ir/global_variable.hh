#pragma once
#include <string>
#include "value.hh"
#include "ilist.hh"
class Module;
class Type;
class Constant;
class GlobalVariable:public Value, public ilist<GlobalVariable>::node{
    public:
        GlobalVariable()=default;
        ~GlobalVariable()=default;
        static GlobalVariable* get(Type* type, Constant* init, std::string &name, Module* parent);
        Constant* get_init() const {return _init;};
    private:
        GlobalVariable(Type* type, Constant* init, std::string &name, Module* parent);
        Constant* _init;
        Module* _parent;
};