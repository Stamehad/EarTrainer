#pragma once
#include "ear/track_selector.hpp" // your existing TrackPhaseCatalog/descriptor
#include <string_view>
#include <vector>

namespace ear::adaptive {
TrackPhaseCatalog load_track_phase_catalog_builtin(std::string_view catalog_name);
std::vector<TrackPhaseCatalog> load_track_phase_catalogs_builtin(const std::vector<std::string_view>& names);
}