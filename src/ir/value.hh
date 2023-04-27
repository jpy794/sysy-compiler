#pragma once
#include<string>
class Type;
class Value{
    public:
        Value()=default;

        Value(Type* type, const std::string &name);

        ~Value()=default;

        Type* get_type() const { return _type; };

        const std::string& get_name() const { return _name; }
        
        void set_name(const std::string &name);
    private:
        Type* _type;
        std::string _name;
};