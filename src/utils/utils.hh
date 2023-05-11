#pragma once

#include <algorithm>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>

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

template <typename Key, typename... Args>
bool contains(std::unordered_map<Key, Args...> &con, const Key &key) {
    return con.find(key) != con.end();
}

template <typename Container, typename Elem>
bool contains(const Container &con, const Elem &elem) {
    static_assert(std::is_same<typename Container::value_type, Elem>::value);
    return std::find(con.begin(), con.end(), elem) != con.end();
}
