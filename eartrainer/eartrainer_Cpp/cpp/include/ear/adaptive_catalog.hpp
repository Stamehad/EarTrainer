#pragma once

#include "drill_spec.hpp"

#include <string>
#include <vector>

namespace ear {
namespace adaptive {

std::vector<DrillSpec> load_level_catalog(const std::string& catalog_path, int level);

} // namespace adaptive
} // namespace ear
