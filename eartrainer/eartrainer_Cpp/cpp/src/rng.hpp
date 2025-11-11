#pragma once

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace ear {

inline std::uint64_t advance_rng(std::uint64_t& state) {
  if (state == 0) {
    state = 0x2545F4914F6CDD1DULL;
  }
  std::uint64_t x = state;
  x ^= x << 13;
  x ^= x >> 7;
  x ^= x << 17;
  state = x;
  return x;
}

inline int rand_int(std::uint64_t& state, int min, int max) {
  if (max < min) {
    throw std::invalid_argument("rand_int: invalid interval [" + std::to_string(min) + "," +
                                std::to_string(max) + "]");
  }
  auto span = static_cast<std::uint64_t>(max - min + 1);
  auto value = advance_rng(state);
  return min + static_cast<int>(value % span);
}

inline double rand_unit(std::uint64_t& state) {
  constexpr double denom = static_cast<double>(std::numeric_limits<std::uint64_t>::max());
  return static_cast<double>(advance_rng(state)) / denom;
}

} // namespace ear
