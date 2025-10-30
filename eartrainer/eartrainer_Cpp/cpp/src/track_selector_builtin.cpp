#include "resources/track_selector_builtin.hpp"
#include "resources/builtin_degree_levels.hpp"
#include "resources/builtin_melody_levels.hpp"
#include "resources/builtin_chord_levels.hpp"
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
  if (name == ChordLevels::name)
    return make_from(name, ChordLevels::phases());
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

namespace {
int tens_digit(int level) {
  if (level < 0) level = -level;
  return (level / 10) % 10;
}
}

TrackSelectionResult compute_track_phase_weights(
    const std::vector<int>& current_levels,
    const std::vector<TrackPhaseCatalog>& catalogs) {
  if (current_levels.size() != catalogs.size()) {
    throw std::invalid_argument("Track selector: current level vector size does not match catalogs");
  }

  TrackSelectionResult result;
  result.weights.assign(current_levels.size(), 0);

  std::optional<int> active_phase;
  for (int level : current_levels) {
    if (level <= 0) continue;
    int level_phase = tens_digit(level);
    if (!active_phase || level_phase < *active_phase) {
      active_phase = level_phase;
    }
  }

  if (!active_phase.has_value()) {
    return result;
  }
  result.phase_digit = *active_phase;

  for (std::size_t i = 0; i < current_levels.size(); ++i) {
    int level = current_levels[i];
    if (level <= 0) {
      continue;
    }

    const auto& summary = catalogs[i];
    int level_phase = tens_digit(level);
    if (level_phase > result.phase_digit) {
      continue;
    }

    auto phase_it = summary.phases.find(result.phase_digit);
    if (phase_it == summary.phases.end() || phase_it->second.empty()) {
      if (level_phase == result.phase_digit) {
        throw std::runtime_error(
            "Track selector: no levels found in active phase for track '" + summary.descriptor.name + "'");
      }
      continue;
    }

    const auto& phase_levels = phase_it->second;
    if (level_phase < result.phase_digit) {
      result.weights[i] = static_cast<int>(phase_levels.size());
      continue;
    }

    auto it = std::lower_bound(phase_levels.begin(), phase_levels.end(), level);
    if (it == phase_levels.end() || *it != level) {
      throw std::runtime_error(
          "Track selector: current level " + std::to_string(level) +
          " not found in catalog for track '" + summary.descriptor.name + "'");
    }

    result.weights[i] = static_cast<int>(phase_levels.end() - it);
  }

  return result;
}
} // namespace ear::adaptive
