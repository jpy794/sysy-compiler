#pragma once

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

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
bool contains(const std::map<Key, Args...> &con, const Key &key) {
    return con.find(key) != con.end();
}

template <typename Key, typename... Args>
bool contains(const std::unordered_map<Key, Args...> &con, const Key &key) {
    return con.find(key) != con.end();
}

template <typename Key, typename... Args>
bool contains(const std::set<Key, Args...> &con, const Key &key) {
    return con.find(key) != con.end();
}

template <typename Key, typename... Args>
bool contains(const std::unordered_set<Key, Args...> &con, const Key &key) {
    return con.find(key) != con.end();
}

template <typename Container, typename Elem>
bool contains(const Container &con, const Elem &elem) {
    static_assert(std::is_same<typename Container::value_type, Elem>::value);
    return std::find(con.begin(), con.end(), elem) != con.end();
}

std::string demangle(const char *mangled_name);
