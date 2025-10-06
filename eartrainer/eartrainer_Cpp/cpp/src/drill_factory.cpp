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

using nlohmann::json;

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

// ------------------------ DrillFactory impl ------------------------

namespace ear {

DrillFactory& DrillFactory::instance() {
  static DrillFactory f;
  return f;
}

// Utility: fill SessionSpec from DrillSpec.defaults + params
namespace {

void fill_session_from_spec(const DrillSpec& spec, SessionSpec& dst) {
  if (spec.defaults.contains("tempo_bpm")) {
    dst.tempo_bpm = std::max(1, spec.defaults["tempo_bpm"].get<int>());
  }
  if (spec.defaults.contains("key")) {
    dst.key = spec.defaults["key"].get<std::string>();
  }
  if (spec.defaults.contains("range_min")) dst.range_min = spec.defaults["range_min"].get<int>();
  if (spec.defaults.contains("range_max")) dst.range_max = spec.defaults["range_max"].get<int>();
  if (spec.defaults.contains("assistance_policy")) {
    dst.assistance_policy.clear();
    const auto& policy = spec.defaults["assistance_policy"];
    if (policy.is_object()) {
      for (const auto& item : policy.get_object()) {
        dst.assistance_policy[item.first] = item.second.get<int>();
      }
    }
  }
  dst.sampler_params = spec.sampler_params;
}

} // namespace

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
  fill_session_from_spec(spec, assignment.spec);
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
