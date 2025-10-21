#pragma once

#include "drill_spec.hpp"

#include "../nlohmann/json.hpp"

#include <string>
#include <vector>

namespace ear {
namespace adaptive {

std::vector<DrillSpec> load_level_catalog(const std::string& catalog_path, int level);
std::vector<DrillSpec> parse_catalog_document(const nlohmann::json& document);
std::vector<DrillSpec> filter_catalog_by_level(const std::vector<DrillSpec>& specs, int level);

} // namespace adaptive
} // namespace ear
