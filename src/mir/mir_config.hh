#pragma once

#include "constant.hh"
#include <variant>

namespace mir {

const size_t TARGET_MACHINE_SIZE = 8;
const size_t SP_ALIGNMENT = 16;

const size_t BASIC_TYPE_SIZE = 4;
const size_t POINTER_TYPE_SIZE = TARGET_MACHINE_SIZE;

const size_t BASIC_TYPE_ALIGN = BASIC_TYPE_SIZE;
const size_t POINTER_TYPE_ALIGN = POINTER_TYPE_SIZE;

enum class BasicType { VOID, INT, FLOAT };

using InitPairs = std::vector<std::pair<unsigned, std::variant<int, float>>>;

void flatten_array(ir::ConstArray *const_arr, InitPairs &inits,
                   const size_t start = 0);

inline size_t ALIGN(size_t x, size_t alignment) {
    return ((x + (alignment - 1)) & ~(alignment - 1));
}

inline size_t SP_ALIGN(size_t x) { return ALIGN(x, SP_ALIGNMENT); }

}; // namespace mir
