#include "../include/ear/track_selector.hpp"

#include "../include/ear/drill_spec.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>

#include "../include/nlohmann/json.hpp"

namespace ear::adaptive {
namespace {

int tens_digit(int level) {
  if (level < 0) level = -level;
  return (level / 10) % 10;
}

std::filesystem::path resolve_catalog_path(const std::filesystem::path& path) {
  if (path.is_absolute()) {
    return path;
  }
  return std::filesystem::current_path() / path;
}

std::vector<DrillSpec> load_catalog_specs(const std::filesystem::path& catalog_path) {
  std::filesystem::path path = catalog_path;
  if (!std::filesystem::exists(path)) {
    if (path.extension() == ".yml") {
      auto alt = path;
      alt.replace_extension(".json");
      if (std::filesystem::exists(alt)) {
        path = alt;
      }
    }
  }

  if (!std::filesystem::exists(path)) {
    throw std::runtime_error("Adaptive track catalog not found at: " + catalog_path.string());
  }

  if (path.extension() != ".json") {
    throw std::runtime_error("Adaptive track catalog must be JSON: " + path.string());
  }

  std::ifstream stream(path);
  if (!stream) {
    throw std::runtime_error("Failed to open track catalog: " + path.string());
  }
  std::string content((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
  auto document = nlohmann::json::parse(content);
  return DrillSpec::load_json(document);
}

} // namespace

const std::vector<TrackCatalogDescriptor>& default_track_catalogs() {
  static const std::vector<TrackCatalogDescriptor> descriptors = {
      {"degree", std::filesystem::path("resources") / "degree_levels.json"},
      {"melody", std::filesystem::path("resources") / "melody_levels.json"},
      {"chord",  std::filesystem::path("resources") / "chord_levels.json"},
  };
  return descriptors;
}

std::vector<TrackCatalogDescriptor> track_catalogs_from_resources(const std::filesystem::path& resources_dir) {
  return {
      {"degree", resources_dir / "degree_levels.json"},
      {"melody", resources_dir / "melody_levels.json"},
      {"chord",  resources_dir / "chord_levels.json"},
  };
}

TrackSelectionResult compute_track_phase_weights(
    const std::vector<int>& current_levels,
    const std::vector<TrackCatalogDescriptor>& catalogs) {
  auto summaries = load_track_phase_catalogs(catalogs);
  return compute_track_phase_weights(current_levels, summaries);
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

TrackSelectionResult compute_track_phase_weights(const std::vector<int>& current_levels) {
  return compute_track_phase_weights(current_levels, default_track_catalogs());
}

TrackPhaseCatalog load_track_phase_catalog(const TrackCatalogDescriptor& descriptor) {
  TrackPhaseCatalog summary;
  summary.descriptor = descriptor;
  summary.resolved_path = resolve_catalog_path(descriptor.catalog_path);

  auto specs = load_catalog_specs(summary.resolved_path);
  std::map<int, std::set<int>> phase_sets;
  for (const auto& spec : specs) {
    phase_sets[tens_digit(spec.level)].insert(spec.level);
  }

  for (auto& [phase, values] : phase_sets) {
    std::vector<int> sorted(values.begin(), values.end());
    summary.phases.emplace(phase, std::move(sorted));
  }

  return summary;
}

std::vector<TrackPhaseCatalog> load_track_phase_catalogs(
    const std::vector<TrackCatalogDescriptor>& descriptors) {
  std::vector<TrackPhaseCatalog> summaries;
  summaries.reserve(descriptors.size());
  for (const auto& descriptor : descriptors) {
    summaries.push_back(load_track_phase_catalog(descriptor));
  }
  return summaries;
}

} // namespace ear::adaptive
