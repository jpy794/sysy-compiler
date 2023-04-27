#include "value.hh"

Value::Value(Type* type, const std::string &name="")
    : _type(type), _name(name) {}
void Value::set_name(const std::string &name){
    _name=name;
}