#pragma once
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace YAML {
class Node;
}

// Forward declare your SessionSpec – include the real header in .cpp.
struct SessionSpec;

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
  int         level = 0;
  nlohmann::json defaults;        // core SessionSpec defaults (tempo, key, range, assistance...)
  nlohmann::json drill_params;    // family-specific knobs
  nlohmann::json sampler_params;  // generator knobs

  // Lightweight helpers
  bool has_default(const char* k) const { return defaults.contains(k); }
  template <class T> T def_as(const char* k, const T& fallback) const {
    if (!defaults.contains(k)) return fallback;
    return defaults[k].get<T>();
  }

  // --- YAML helpers ---
  static DrillSpec from_yaml(const YAML::Node& n);
  static std::vector<DrillSpec> load_yaml(const std::string& path_or_yaml);
  static std::vector<DrillSpec> filter_by_level(const std::vector<DrillSpec>& all, int level);
};
