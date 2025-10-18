// src/track_selector_builtin.cpp
#include "resources/track_selector_builtin.hpp"
#include "resources/builtin_degree_levels.hpp"
#include "resources/builtin_melody_levels.hpp"
//#include "resources/builtin_chord_levels.hpp"
//#include "resources/builtin_interval_levels.hpp"

#include <utility>

namespace ear::adaptive {
static TrackPhaseCatalog make_from(std::string_view name, const std::map<int,std::vector<int>>& phases) {
  TrackPhaseCatalog c;
  c.descriptor.name = std::string(name);
  c.descriptor.catalog_path.clear(); // not used in builtin mode
  c.phases = phases;
  return c;
}

// TrackPhaseCatalog load_track_phase_catalog_builtin(std::string_view name) {
//   if (name == ear::builtin::DegreeLevels::name)
//     return make_from(name, ear::builtin::DegreeLevels::phases());
//   return {};
// }

TrackPhaseCatalog load_track_phase_catalog_builtin(std::string_view name) {
  using namespace ear::builtin;
  if (name == DegreeLevels::name)
    return make_from(name, DegreeLevels::phases());
  if (name == MelodyLevels::name)
    return make_from(name, MelodyLevels::phases());
//   if (name == ChordLevels::name)
//     return make_from(name, ChordLevels::phases());
//   if (name == IntervalLevels::name)
//     return make_from(name, IntervalLevels::phases());
  return {};
}

std::vector<TrackPhaseCatalog> load_track_phase_catalogs_builtin(const std::vector<std::string_view>& names) {
  std::vector<TrackPhaseCatalog> out;
  out.reserve(names.size());
  for (auto n : names) {
    auto catalog = load_track_phase_catalog_builtin(n);
    if (!catalog.phases.empty()) {
      out.push_back(std::move(catalog));
    }
  }
  return out;
}
} // namespace ear::adaptive
