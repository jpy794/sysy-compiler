#pragma once

#include "constant.hh"
#include <variant>

namespace mir {

const size_t BASIC_TYPE_SIZE = 4;
const size_t POINTER_TYPE_SIZE = 8;

const size_t BASIC_TYPE_ALIGN = BASIC_TYPE_SIZE;
const size_t POINTER_TYPE_ALIGN = POINTER_TYPE_SIZE;

enum class BasicType { VOID, INT, FLOAT };

using InitPairs = std::vector<std::pair<unsigned, std::variant<int, float>>>;

void flatten_array(ir::ConstArray *const_arr, InitPairs &inits,
                   const size_t start = 0);
}; // namespace mir
