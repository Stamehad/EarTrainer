#include "../include/ear/drill_factory.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <variant>
#include <utility>

#include "../drills/common.hpp"        // parse_key_from_string etc.
#include "../drills/chord.hpp"
#include "../drills/interval.hpp"
#include "../drills/melody.hpp"
#include "../drills/note.hpp"
#include "../drills/harmony.hpp"
#include "../include/ear/types.hpp"
#include "../include/ear/midi_clip.hpp"


DrillSpec DrillSpec::from_session(const ear::SessionSpec& spec) {
  DrillSpec out;
  out.id = spec.drill_kind;
  out.family = spec.drill_kind;
  out.level = spec.inspect_level;
  out.tier = spec.inspect_tier;
  out.key = spec.key;
  out.params = spec.params;
  out.assistance_policy = spec.assistance_policy;
  out.defaults["key"] = out.key;
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
  try {
    assignment.module->configure(assignment.spec);
  } catch (const std::bad_variant_access& ex) {
    throw std::runtime_error("DrillFactory: bad params variant for family '" + spec.family +
                             "' (id=" + spec.id + ") : " + ex.what());
  }
  return assignment;
}

std::vector<DrillAssignment> DrillFactory::create_for_level(const std::vector<DrillSpec>& all, int level) const {
  std::vector<DrillSpec> specs;
  for (const auto& spec : all) {
    if (spec.level == level) {
      specs.push_back(spec);
    }
  }
  std::vector<DrillAssignment> out;
  out.reserve(specs.size());
  for (const auto& s : specs) out.push_back(create(s));
  return out;
}

namespace {

} // namespace

// ------------------------ Concrete bindings ------------------------

void register_builtin_drills(DrillFactory& factory) {
  factory.register_family("melody", []() { return std::make_unique<MelodyDrill>(); });

  factory.register_family("note", []() { return std::make_unique<NoteDrill>(); });

  factory.register_family("interval", []() { return std::make_unique<IntervalDrill>(); });

  factory.register_family("chord", []() { return std::make_unique<ChordDrill>(); });
  factory.register_family("chord_melody", []() { return std::make_unique<ChordDrill>(); });
  factory.register_family("harmony", []() { return std::make_unique<HarmonyDrill>(); });
}

} // namespace ear
