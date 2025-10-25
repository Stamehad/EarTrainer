#pragma once
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "../resources/drill_params.hpp"
#include "../nlohmann/json.hpp"

// SessionSpec is defined in ear/types.hpp; needed for conversion helpers.
#include "types.hpp"

/**
 * DrillSpec: data-only description of one drill template.
 * Flexible enough for melody, chords, intervals, mixed.
 */
struct DrillSpec {
  std::string id;
  std::string family;
  std::optional<int> level;
  std::optional<int> tier;

  // Resolved configuration (available to drills without extra parsing).
  std::string key = "C";
  std::unordered_map<std::string, int> assistance_policy;
  ear::DrillParams params{};
  nlohmann::json j_params = nlohmann::json::object();

  // Raw authoring data (kept for serialization/backward compatibility).
  nlohmann::json defaults;        // core defaults (tempo, key, range, assistance...)

  // Convenience helpers on raw defaults
  bool has_default(const char* k) const { return defaults.contains(k); }
  template <class T> T def_as(const char* k, const T& fallback) const {
    if (!defaults.contains(k)) return fallback;
    return defaults[k].get<T>();
  }

  void apply_defaults();

  static DrillSpec from_session(const ear::SessionSpec& spec);
};
