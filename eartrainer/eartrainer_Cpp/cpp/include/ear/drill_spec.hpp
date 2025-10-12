#pragma once
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "../nlohmann/json.hpp"

namespace YAML {
class Node;
}

// SessionSpec is defined in ear/types.hpp; needed for conversion helpers.
#include "types.hpp"

/**
 * DrillSpec: data-only description of one drill template, loaded from YAML.
 * Flexible enough for melody, chords, intervals, mixed.
 *
 * Minimal schema (YAML):
 * drills:
 *  - id: M_Single_Pathway_60
 *    family: melody           # "melody" | "note" | "interval_harmonic" | "chord" | ...
 *    level: 1                 # integer tag used by DrillHub to pick a bout set
 *    defaults:                # OPTIONAL: core SessionSpec defaults
 *      tempo_bpm: 60
 *      key: C_major
 *      range_min: 60
 *      range_max: 72
 *      assistance_policy: { Replay: 2, TempoDown: 1 }
 *    drill_params:            # OPTIONAL: family-specific knobs
 *      cue_type: pathway_immediate
 *    sampler_params:          # OPTIONAL: generator knobs
 *      melody_length: 1
 */
struct DrillSpec {
  std::string id;
  std::string family;
  int level = 0;

  // Resolved configuration (available to drills without extra parsing).
  std::string key = "C";
  int range_min = 60;
  int range_max = 72;
  std::optional<int> tempo_bpm;
  std::unordered_map<std::string, int> assistance_policy;
  nlohmann::json params = nlohmann::json::object();

  // Raw authoring data (kept for serialization/backward compatibility).
  nlohmann::json defaults;        // core defaults (tempo, key, range, assistance...)
  nlohmann::json drill_params;    // family-specific knobs (legacy)
  nlohmann::json sampler_params;  // legacy generator knobs

  // Convenience helpers on raw defaults
  bool has_default(const char* k) const { return defaults.contains(k); }
  template <class T> T def_as(const char* k, const T& fallback) const {
    if (!defaults.contains(k)) return fallback;
    return defaults[k].get<T>();
  }

  void apply_defaults();

  static DrillSpec from_yaml(const YAML::Node& n);
  static std::vector<DrillSpec> load_yaml(const std::string& path_or_yaml);
  static DrillSpec from_json(const nlohmann::json& spec_json);
  static std::vector<DrillSpec> load_json(const nlohmann::json& document);
  static std::vector<DrillSpec> filter_by_level(const std::vector<DrillSpec>& all, int level);
  static DrillSpec from_session(const ear::SessionSpec& spec);
};
