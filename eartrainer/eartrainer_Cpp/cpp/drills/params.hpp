#pragma once

#include "../include/ear/drill_spec.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace ear::drills {

// inline bool param_flag(const DrillSpec& spec, const std::string& key, bool fallback) {
//   if (!spec.params.is_object() || !spec.params.contains(key)) {
//     return fallback;
//   }
//   const auto& node = spec.params[key];
//   if (node.is_boolean()) {
//     return node.get<bool>();
//   }
//   if (node.is_number_integer()) {
//     return node.get<int>() != 0;
//   }
//   return fallback;
// }

// inline int param_int(const DrillSpec& spec, const std::string& key, int fallback) {
//   if (!spec.params.is_object() || !spec.params.contains(key)) {
//     return fallback;
//   }
//   const auto& node = spec.params[key];
//   if (node.is_number_integer()) {
//     return node.get<int>();
//   }
//   if (node.is_number()) {
//     return static_cast<int>(node.get<double>());
//   }
//   return fallback;
// }

// inline double param_double(const DrillSpec& spec, const std::string& key, double fallback) {
//   if (!spec.params.is_object() || !spec.params.contains(key)) {
//     return fallback;
//   }
//   const auto& node = spec.params[key];
//   if (node.is_number_float()) {
//     return node.get<double>();
//   }
//   if (node.is_number_integer()) {
//     return static_cast<double>(node.get<int>());
//   }
//   return fallback;
// }

// inline std::vector<int> param_int_list(const DrillSpec& spec, const std::string& key) {
//   std::vector<int> values;
//   if (!spec.params.is_object() || !spec.params.contains(key)) {
//     return values;
//   }
//   const auto& node = spec.params[key];
//   if (!node.is_array()) {
//     return values;
//   }
//   for (const auto& entry : node.get_array()) {
//     if (entry.is_number_integer()) {
//       values.push_back(entry.get<int>());
//     }
//   }
//   std::sort(values.begin(), values.end());
//   values.erase(std::unique(values.begin(), values.end()), values.end());
//   return values;
// }

inline void ensure_object(nlohmann::json& node) {
  if (!node.is_object()) {
    node = nlohmann::json::object();
  }
}

// inline int param_tempo(const DrillSpec& spec, const std::string& key, int fallback) {
//   if (spec.params.is_object() && spec.params.contains(key)) {
//     const auto& node = spec.params[key];
//     if (node.is_number_float()) {
//       return std::max(1, static_cast<int>(std::lround(node.get<double>())));
//     }
//     if (node.is_number_integer()) {
//       return std::max(1, node.get<int>());
//     }
//   }
//   if (spec.tempo_bpm.has_value()) {
//     return std::max(1, spec.tempo_bpm.value());
//   }
//   return std::max(1, fallback);
// }

inline int beats_to_ms(double beats, int tempo_bpm) {
  if (beats <= 0.0) {
    return 0;
  }
  int tempo = std::max(1, tempo_bpm);
  double quarter_ms = 60000.0 / static_cast<double>(tempo);
  return static_cast<int>(std::lround(quarter_ms * beats));
}

} // namespace ear::drills
