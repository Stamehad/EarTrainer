#include "../include/ear/adaptive_catalog.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>

#include "../include/nlohmann/json.hpp"

namespace ear::adaptive {

namespace {

} // namespace

// std::vector<DrillSpec> parse_catalog_document(const nlohmann::json& document) {
//   const nlohmann::json* drills_json = &document;
//   if (document.is_object()) {
//     if (!document.contains("drills")) {
//       throw std::runtime_error("Adaptive catalog JSON must contain a 'drills' array");
//     }
//     drills_json = &document["drills"];
//   }

//   if (!drills_json->is_array()) {
//     throw std::runtime_error("Adaptive catalog JSON must contain a 'drills' array");
//   }

//   const auto& entries = drills_json->get_array();
//   std::vector<DrillSpec> specs;
//   specs.reserve(entries.size());
//   for (const auto& entry : entries) {
//     specs.push_back(spec_from_json(entry));
//   }
//   return specs;
// }

std::vector<DrillSpec> filter_catalog_by_level(const std::vector<DrillSpec>& specs, int level) {
  std::vector<DrillSpec> out;
  for (const auto& spec : specs) {
    if (spec.level == level) {
      out.push_back(spec);
    }
  }
  return out;
}

// std::vector<DrillSpec> load_level_catalog(const std::string& catalog_path, int level) {
//   if (catalog_path.empty()) {
//     throw std::invalid_argument("Adaptive catalog path is empty");
//   }

//   std::filesystem::path path{catalog_path};
//   if (!path.is_absolute()) {
//     path = std::filesystem::current_path() / path;
//   }
//   if (!std::filesystem::exists(path)) {
//     if (path.extension() == ".yml") {
//       auto alt = path;
//       alt.replace_extension(".json");
//       if (std::filesystem::exists(alt)) {
//         path = alt;
//       }
//     }
//   }

//   if (!std::filesystem::exists(path)) {
//     throw std::runtime_error("Adaptive catalog not found at: " + path.string());
//   }

//   if (path.extension() != ".json") {
//     throw std::runtime_error("Adaptive catalog must be provided as JSON: " + path.string());
//   }

//   std::ifstream stream(path);
//   if (!stream) {
//     throw std::runtime_error("Failed to open adaptive catalog: " + path.string());
//   }
//   std::string content((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
//   auto document = nlohmann::json::parse(content);
//   auto all_specs = parse_catalog_document(document);
//   auto filtered = filter_catalog_by_level(all_specs, level);
//   if (filtered.empty()) {
//     throw std::runtime_error("No adaptive drills configured for level " + std::to_string(level));
//   }
//   return filtered;
// }

} // namespace ear::adaptive
