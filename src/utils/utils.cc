#include "utils.hh"

#include <cxxabi.h>

using namespace std;

string demangle(const char *mangled_name) {
    auto name = abi::__cxa_demangle(mangled_name, nullptr, 0, nullptr);
    if (name == nullptr) {
        return mangled_name;
    }
    string ret{name};
    free(name);
    return ret;
}
