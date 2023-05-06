#pragma once

#include <stdexcept>
#include <type_traits>

template <typename Derived, typename Base> bool is_a(Base *base) {
    static_assert(std::is_base_of<Base, Derived>::value);
    return dynamic_cast<Derived *>(base) != nullptr;
}

template <typename Derived, typename Base> Derived *as_a(Base *base) {
    static_assert(std::is_base_of<Base, Derived>::value);
    auto derived = dynamic_cast<Derived *>(base);
    if (not derived) {
        throw std::logic_error{"bad asa"};
    }
    return derived;
}

template <typename Container, typename Key>
bool contains(Container &con, const Key &key) {
    static_assert(std::is_same<typename Container::key_type, Key>::value);
    return con.find(key) != con.end();
}
