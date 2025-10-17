#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace ear::adaptive {

struct TrackCatalogDescriptor {
  std::string name;
  std::filesystem::path catalog_path;
};

struct TrackSelectionResult {
  int phase_digit = -1;
  std::vector<int> weights;
};

struct TrackPhaseCatalog {
  TrackCatalogDescriptor descriptor;
  std::filesystem::path resolved_path;
  std::map<int, std::vector<int>> phases;
};

const std::vector<TrackCatalogDescriptor>& default_track_catalogs();
std::vector<TrackCatalogDescriptor> track_catalogs_from_resources(const std::filesystem::path& resources_dir);

TrackSelectionResult compute_track_phase_weights(
    const std::vector<int>& current_levels,
    const std::vector<TrackCatalogDescriptor>& catalogs);

TrackSelectionResult compute_track_phase_weights(
    const std::vector<int>& current_levels,
    const std::vector<TrackPhaseCatalog>& catalogs);

TrackPhaseCatalog load_track_phase_catalog(const TrackCatalogDescriptor& descriptor);
std::vector<TrackPhaseCatalog> load_track_phase_catalogs(
    const std::vector<TrackCatalogDescriptor>& descriptors);

TrackSelectionResult compute_track_phase_weights(const std::vector<int>& current_levels);

// Parallel API (no files)
TrackPhaseCatalog load_track_phase_catalog_builtin(std::string_view name);
std::vector<TrackPhaseCatalog> load_track_phase_catalogs_builtin(const std::vector<std::string_view>& names);

} // namespace ear::adaptive
