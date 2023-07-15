#include <fstream>
#include <ios>

#ifndef NDEBUG
constexpr bool debug = true;
#else
constexpr bool debug = false;
#endif

class DebugStream {
  private:
    std::ofstream of{"log.txt", std::ios::app};

  public:
    template <typename T> DebugStream &operator<<(const T &rhs) {
        if constexpr (debug) {
            of << rhs;
            of.flush();
        }
        return *this;
    }
};

extern DebugStream debugs;
