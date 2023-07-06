#pragma once

#include <stdexcept>

class unreachable_error : public std::logic_error {
  public:
    unreachable_error() : logic_error{"unreachable code"} {}
    unreachable_error(const std::string &what_arg) : logic_error{what_arg} {}
    const char *what() const noexcept final { return logic_error::what(); }
};

class not_implemented_error : public std::logic_error {
  public:
    not_implemented_error()
        : std::logic_error("Function not yet implemented"){};
};
