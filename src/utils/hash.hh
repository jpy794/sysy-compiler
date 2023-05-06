#pragma once

#include <functional>
#include <vector>

// see https://stackoverflow.com/a/72071056
// from boost (functional/hash):
// see http://www.boost.org/doc/libs/1_35_0/doc/html/hash/combine.html
template <class T> inline void hash_combine(size_t &seed, T const &v) {
    seed ^= std::hash<T>()(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

template <class T>
inline void hash_combine(size_t &seed, std::vector<T> const &v) {
    for (auto &&i : v) {
        hash_combine(seed, i);
    }
}

struct VectorHash {
    template <class T> std::size_t operator()(const std::vector<T> &p) const {
        size_t seed{0};
        hash_combine(seed, p);
        return seed;
    }
};

struct PairHash {
    template <class T1, class T2>
    std::size_t operator()(const std::pair<T1, T2> &p) const {
        size_t seed{0};
        hash_combine(seed, p.first);
        hash_combine(seed, p.second);
        return seed;
    }
};