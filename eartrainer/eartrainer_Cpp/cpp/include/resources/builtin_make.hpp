#pragma once
#include "ear/drill_spec.hpp"
#include <nlohmann/json.hpp>
#include <unordered_map>

namespace ear::builtin {

// Minimal, parser-free constructor. Only set what you need.
// key/range_min/range_max are intentionally omitted.
inline DrillSpec make_drill(
    std::string id,
    std::string family,          // "note", "melody", "chord", ...
    int level,
    std::optional<int> tempo_bpm,        // if you want a default tempo
    nlohmann::json drill_params,         // your existing knobs
    nlohmann::json sampler_params = {},  // optional; keep shape you already use
    std::unordered_map<std::string,int> assistance = {}
){
    DrillSpec d{};
    d.id      = std::move(id);
    d.family  = std::move(family);
    d.level   = level;

    // Keep your current shapes so downstream code is untouched.
    d.defaults = nlohmann::json::object();
    if (tempo_bpm) d.defaults["tempo_bpm"] = *tempo_bpm;
    if (!assistance.empty()) {
      nlohmann::json ap = nlohmann::json::object();
      for (const auto& kv : assistance) {
        ap[kv.first] = kv.second;
      }
      d.defaults["assistance_policy"] = std::move(ap);
    }

    d.drill_params   = std::move(drill_params);
    d.sampler_params = std::move(sampler_params);

    // Intentionally NOT setting: d.key, d.range_min, d.range_max.
    // Your adaptive code can/randomly will decide them at runtime.
    return d;
}

inline nlohmann::json jarray(std::initializer_list<int> xs) {
  nlohmann::json a = nlohmann::json::array();
  for (int v : xs) a.push_back(v);
  return a;
}

} // namespace ear::builtin