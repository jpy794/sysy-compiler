#ifndef IR_MODULE
#define IR_MODULE

#include <string>
using std::string;

class Module {
public:
    Module(string& name);
    ~Module() = default;

private:
    std::string module_name_;
};

#endif
