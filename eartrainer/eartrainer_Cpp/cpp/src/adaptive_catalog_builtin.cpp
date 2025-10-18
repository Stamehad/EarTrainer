// src/adaptive_catalog_builtin.cpp
#include "resources/adaptive_catalog_builtin.hpp"
#include "resources/builtin_melody_levels.hpp"
#include "resources/builtin_degree_levels.hpp"

namespace ear::adaptive {

struct CatalogRegistration {
  std::string_view name;
  const std::vector<DrillSpec>& (*drills_for_level)(int);
  const std::vector<int>& (*known_levels)();
};

static const CatalogRegistration kCatalogs[] = {
    {ear::builtin::MelodyLevels::name,
     ear::builtin::MelodyLevels::drills_for_level,
     ear::builtin::MelodyLevels::known_levels},
    {ear::builtin::DegreeLevels::name,
     ear::builtin::DegreeLevels::drills_for_level,
     ear::builtin::DegreeLevels::known_levels},
    // Future: chord/interval catalogs
};

std::vector<DrillSpec> load_level_catalog_builtin(std::string_view catalog_name, int level) {
  for (const auto& reg : kCatalogs) {
    if (reg.name == catalog_name) {
      return reg.drills_for_level(level);
    }
  }
  return {};
}

std::vector<DrillSpec> load_phase_catalog_builtin(std::string_view catalog_name, int phase) {
  std::vector<DrillSpec> out;
  for (const auto& reg : kCatalogs) {
    if (reg.name != catalog_name) {
      continue;
    }
    for (int level : reg.known_levels()) {
      if (phase_of_level(level) != phase) {
        continue;
      }
      const auto& drills = reg.drills_for_level(level);
      out.insert(out.end(), drills.begin(), drills.end());
    }
  }
  return out;
}

} // namespace ear::adaptive
