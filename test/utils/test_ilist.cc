#include "ilist.hh"
#include <algorithm>
#include <iostream>
#include <stdexcept>

class inst : public ilist<inst>::node {
  public:
    std::string inst_str;
    inst(const std::string &s) : inst_str(s) {}

    bool operator==(const inst &rhs) { return inst_str == rhs.inst_str; }
};

int main() {
    ilist<inst> inst_list;
    inst_list.emplace_back("world");
    inst_list.push_front(new inst{"hello"});
    inst_list.push_back(new inst{"!"});
    for (auto &&inst : inst_list) {
        std::cout << inst.inst_str << '\n';
    }

    auto &front = inst_list.front();
    std::cout << "erasing " << front.inst_str << '\n';
    inst_list.erase(&front);

    inst_list.emplace(&inst_list.back(), ":D");

    for (auto &&inst : inst_list) {
        std::cout << inst.inst_str << '\n';
    }

    auto it = std::find(inst_list.begin(), inst_list.end(), inst{":D"});
    inst_list.emplace(it, "I'm");

    bool is_except{false};
    try {
        inst_list.erase(new inst{"hi"});
    } catch (std::logic_error &e) {
        is_except = true;
    }
    if (not is_except) {
        throw std::logic_error{"tag test failed"};
    } else {
        std::cout << "tag test passed";
    }

    for (auto &&inst : inst_list) {
        std::cout << inst.inst_str << '\n';
    }
}