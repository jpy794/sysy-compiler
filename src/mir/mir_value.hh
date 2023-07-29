#pragma once

#include <iostream>
#include <unordered_set>

namespace context {
struct Context;
}

namespace mir {

using context::Context;

class Value {
  public:
    virtual void dump(std::ostream &os, const Context &context) const = 0;
    virtual ~Value() {}
};

// FIXME how to AUTO release unused value?
class ValueManager {
    std::unordered_set<Value *> _values;

  public:
    static ValueManager &get() {
        static ValueManager factory;
        return factory;
    }
    ~ValueManager() {
        for (auto v : _values)
            delete v;
    }

    template <class T, typename... Args> T *create(Args... args) {
        static_assert(std::is_base_of<Value, T>::value);
        T *v = new T(args...);
        _values.insert(static_cast<Value *>(v));
        return v;
    }
};

}; // namespace mir
