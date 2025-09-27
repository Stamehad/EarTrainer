#include "pathways.hpp"

#include "common.hpp"

#include <algorithm>
#include <cctype>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace ear::pathways {

namespace {

PathwayBank& mutable_bank() {
  static PathwayBank bank;
  return bank;
}

std::once_flag init_flag;

void ensure_loaded() {
  std::call_once(init_flag, [] {
    // Built-in default pathways for major scale (0-based degrees).
    DegreePathways major;
    auto make = [](std::initializer_list<int> seq) {
      PathwayOptions opt;
      opt.primary.degrees = std::vector<int>(seq);
      return opt;
    };
    major.emplace(0, make({0}));
    major.emplace(1, make({1, 0}));
    major.emplace(2, make({2, 1, 0}));
    major.emplace(3, make({3, 2, 1, 0}));
    major.emplace(4, make({4, 5, 6, 7}));
    major.emplace(5, make({5, 6, 7}));
    major.emplace(6, make({6, 7}));

    PathwayBank bank;
    bank.emplace("major", std::move(major));
    mutable_bank() = std::move(bank);
  });
}

} // namespace

const PathwayBank& default_bank() {
  ensure_loaded();
  return mutable_bank();
}

std::string infer_scale_type(const std::string& key) {
  auto space_pos = key.find(' ');
  std::string mode = space_pos == std::string::npos ? "major" : key.substr(space_pos + 1);
  std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  if (mode.empty()) {
    return "major";
  }
  return mode;
}

const PathwayOptions* find_pathway(const PathwayBank& bank, const std::string& scale_type,
                                   int degree) {
  auto bank_it = bank.find(scale_type);
  if (bank_it == bank.end()) {
    return nullptr;
  }
  const auto& degree_map = bank_it->second;
  auto degree_it = degree_map.find(drills::normalize_degree_index(degree));
  if (degree_it == degree_map.end()) {
    return nullptr;
  }
  return &degree_it->second;
}

} // namespace ear::pathways
