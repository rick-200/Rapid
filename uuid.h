#pragma once
#include <chrono>
#include <cstdint>
#include <random>

#include "util.h"
namespace rapid {
class UUID {
  uint64_t val0, val1;

 public:
  UUID() = delete;
  constexpr UUID(uint64_t val0, uint64_t val1) : val0(val0), val1(val1) {}
  constexpr UUID(const UUID&) = default;
  constexpr UUID(UUID&&) = default;
  UUID& operator=(const UUID&) = default;
  UUID& operator=(UUID&&) = default;

  bool operator==(const UUID& other) const {
    return val0 == other.val0 && val1 == other.val1;
  }
  uint64_t get_val0() const { return val0; }
  uint64_t get_val1() const { return val1; }

 public:
  static UUID Create(const char* author) {
    using namespace std::chrono;
    uint64_t time =
        duration_cast<milliseconds>(system_clock::now().time_since_epoch())
            .count();
    std::random_device rand;
    uint64_t author_hash = hash_string(author, cstring_length(author));
    uint64_t val0 = (time << 16) | (rand() & 0xFFFF);
    uint64_t val1 = (author_hash << 16) | ((rand() & 0xFFFF0000) >> 16);
    return UUID(val0, val1);
  }
};
}  // namespace rapid