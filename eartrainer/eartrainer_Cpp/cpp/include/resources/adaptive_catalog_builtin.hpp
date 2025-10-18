#pragma once
#include "ear/drill_spec.hpp"
#include <string_view>
#include <vector>

namespace ear::adaptive {

// Drop-in parallel to your file-based API:
std::vector<DrillSpec> load_level_catalog_builtin(std::string_view catalog_name, int level);

// Convenience: filter by phase (derived from level’s tens digit)
std::vector<DrillSpec> load_phase_catalog_builtin(std::string_view catalog_name, int phase);

// Tiny helper: phase = 0 if level < 10, else tens digit.
constexpr int phase_of_level(int level) {
    return (level < 10) ? 0 : (level / 10) % 10;
}

} // namespace ear::adaptive