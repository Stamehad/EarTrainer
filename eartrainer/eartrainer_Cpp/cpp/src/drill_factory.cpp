#include "ear/drill_factory.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#if __has_include(<yaml-cpp/yaml.h>)
#define EAR_HAVE_YAML 1
#include <yaml-cpp/yaml.h>
#else
#define EAR_HAVE_YAML 0
#endif

#include "common.hpp"        // parse_key_from_string etc.
#include "drills/chord.hpp"
#include "drills/interval.hpp"
#include "drills/melody.hpp"
#include "drills/note.hpp"
#include "ear/types.hpp"

using nlohmann::json;

void DrillSpec::apply_defaults() {
  key = "C";
  range_min = 60;
  range_max = 72;
  tempo_bpm.reset();
  assistance_policy.clear();
  params = nlohmann::json::object();

  if (defaults.is_object()) {
    const auto& obj = defaults.get_object();
    if (auto it = obj.find("key"); it != obj.end() && it->second.is_string()) {
      key = it->second.get<std::string>();
    }
    if (auto it = obj.find("range_min"); it != obj.end()) {
      range_min = it->second.get<int>();
    }
    if (auto it = obj.find("range_max"); it != obj.end()) {
      range_max = it->second.get<int>();
    }
    if (auto it = obj.find("tempo_bpm"); it != obj.end()) {
      tempo_bpm = std::max(1, it->second.get<int>());
    }
    if (auto it = obj.find("assistance_policy"); it != obj.end() && it->second.is_object()) {
      for (const auto& kv : it->second.get_object()) {
        assistance_policy[kv.first] = kv.second.get<int>();
      }
    }
  }

  if (drill_params.is_object()) {
    for (const auto& kv : drill_params.get_object()) {
      params[kv.first] = kv.second;
    }
  }
  if (sampler_params.is_object()) {
    for (const auto& kv : sampler_params.get_object()) {
      params[kv.first] = kv.second;
    }
  }
}

#if EAR_HAVE_YAML

// ------------------------ DrillSpec YAML impl ------------------------

static json yaml_to_json(const YAML::Node& n) {
  // Minimal recursive conversion of YAML::Node to nlohmann::json
  using Node = YAML::Node;
  if (!n) return json();
  if (n.IsScalar()) {
    // try int, double, bool, then string
    const auto s = n.as<std::string>();
    // naive numeric/bool detection (good enough for our spec)
    try { return json(n.as<int>());   } catch (...) {}
    try { return json(n.as<double>());} catch (...) {}
    if (s == "true" || s == "false") return json(n.as<bool>());
    return json(s);
  }
  if (n.IsSequence()) {
    json arr = json::array();
    for (auto it : n) arr.push_back(yaml_to_json(it));
    return arr;
  }
  if (n.IsMap()) {
    json obj = json::object();
    for (auto it : n) obj[it.first.as<std::string>()] = yaml_to_json(it.second);
    return obj;
  }
  return json();
}

DrillSpec DrillSpec::from_yaml(const YAML::Node& n) {
  DrillSpec s;
  s.id     = n["id"].as<std::string>();
  s.family = n["family"].as<std::string>();
  s.level  = n["level"].as<int>();
  if (n["defaults"])       s.defaults       = yaml_to_json(n["defaults"]);
  if (n["drill_params"])   s.drill_params   = yaml_to_json(n["drill_params"]);
  if (n["sampler_params"]) s.sampler_params = yaml_to_json(n["sampler_params"]);
  s.apply_defaults();
  return s;
}

std::vector<DrillSpec> DrillSpec::load_yaml(const std::string& path_or_yaml) {
  YAML::Node doc;
  // Try file first
  try {
    doc = YAML::LoadFile(path_or_yaml);
  } catch (...) {
    // Otherwise treat as YAML content
    doc = YAML::Load(path_or_yaml);
  }
  std::vector<DrillSpec> out;
  if (doc["drills"]) {
    for (const auto& node : doc["drills"]) out.push_back(DrillSpec::from_yaml(node));
  } else if (doc.IsSequence()) {
    for (const auto& node : doc) out.push_back(DrillSpec::from_yaml(node));
  } else {
    throw std::runtime_error("YAML must contain a 'drills' sequence or be a sequence");
  }
  return out;
}

#else

DrillSpec DrillSpec::from_yaml(const YAML::Node&) {
  throw std::runtime_error("yaml-cpp support not available at build time");
}

std::vector<DrillSpec> DrillSpec::load_yaml(const std::string&) {
  throw std::runtime_error("yaml-cpp support not available at build time");
}

#endif

std::vector<DrillSpec> DrillSpec::filter_by_level(const std::vector<DrillSpec>& all, int level) {
  std::vector<DrillSpec> out;
  for (const auto& s : all) if (s.level == level) out.push_back(s);
  return out;
}

DrillSpec DrillSpec::from_session(const ear::SessionSpec& spec) {
  DrillSpec out;
  out.id = spec.drill_kind;
  out.family = spec.drill_kind;
  out.level = 0;
  out.key = spec.key;
  out.range_min = spec.range_min;
  out.range_max = spec.range_max;
  if (spec.tempo_bpm.has_value()) {
    out.tempo_bpm = spec.tempo_bpm;
  }
  out.assistance_policy = spec.assistance_policy;
  out.params = spec.sampler_params.is_object() ? spec.sampler_params : nlohmann::json::object();
  out.sampler_params = out.params;
  out.drill_params = nlohmann::json::object();
  out.defaults = nlohmann::json::object();
  out.defaults["key"] = out.key;
  out.defaults["range_min"] = out.range_min;
  out.defaults["range_max"] = out.range_max;
  if (out.tempo_bpm.has_value()) {
    out.defaults["tempo_bpm"] = *out.tempo_bpm;
  }
  if (!out.assistance_policy.empty()) {
    nlohmann::json assists = nlohmann::json::object();
    for (const auto& kv : out.assistance_policy) {
      assists[kv.first] = kv.second;
    }
    out.defaults["assistance_policy"] = assists;
  }
  return out;
}

// ------------------------ DrillFactory impl ------------------------

namespace ear {

DrillFactory& DrillFactory::instance() {
  static DrillFactory f;
  return f;
}

void DrillFactory::register_family(const std::string& family, Creator create) {
  registry_[family] = std::move(create);
}

std::unique_ptr<DrillModule> DrillFactory::create_module(const std::string& family) const {
  auto it = registry_.find(family);
  if (it == registry_.end()) {
    throw std::runtime_error("DrillFactory: family not registered: " + family);
  }
  auto module = it->second();
  if (!module) {
    throw std::runtime_error("DrillFactory: creator returned null for family: " + family);
  }
  return module;
}

DrillAssignment DrillFactory::create(const DrillSpec& spec) const {
  DrillAssignment assignment;
  assignment.id = spec.id;
  assignment.family = spec.family;
  assignment.spec = spec;
  assignment.module = create_module(spec.family);
  assignment.module->configure(assignment.spec);
  return assignment;
}

std::vector<DrillAssignment> DrillFactory::create_for_level(const std::vector<DrillSpec>& all, int level) const {
  auto specs = DrillSpec::filter_by_level(all, level);
  std::vector<DrillAssignment> out;
  out.reserve(specs.size());
  for (const auto& s : specs) out.push_back(create(s));
  return out;
}

// ------------------------ Concrete bindings ------------------------

void register_builtin_drills(DrillFactory& factory) {
  factory.register_family("melody", []() { return std::make_unique<MelodyDrill>(); });

  factory.register_family("note", []() { return std::make_unique<NoteDrill>(); });

  factory.register_family("interval", []() { return std::make_unique<IntervalDrill>(); });

  factory.register_family("chord", []() { return std::make_unique<ChordDrill>(); });
  factory.register_family("chord_melody", []() { return std::make_unique<ChordDrill>(); });
}

} // namespace ear
