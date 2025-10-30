#pragma once
#include <map>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>

namespace ear::adaptive {

struct TrackCatalogDescriptor {
  std::string name;
  std::filesystem::path catalog_path; // unused in builtin mode
};

struct TrackSelectionResult {
  int phase_digit = -1;
  std::vector<int> weights;
};

struct TrackPhaseCatalog {
  TrackCatalogDescriptor descriptor;
  std::filesystem::path resolved_path; // unused in builtin mode
  std::map<int, std::vector<int>> phases; // phase_digit -> sorted levels
};

TrackPhaseCatalog load_track_phase_catalog_builtin(std::string_view catalog_name);
std::vector<TrackPhaseCatalog> load_track_phase_catalogs_builtin(const std::vector<std::string_view>& names);

// We still expose a compute API compatible with existing code/tests.
TrackSelectionResult compute_track_phase_weights(
    const std::vector<int>& current_levels,
    const std::vector<TrackPhaseCatalog>& catalogs);

} // namespace ear::adaptive
