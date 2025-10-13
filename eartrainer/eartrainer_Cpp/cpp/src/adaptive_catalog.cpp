#include "../include/ear/adaptive_catalog.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>

#include "../include/nlohmann/json.hpp"

namespace ear::adaptive {

std::vector<DrillSpec> load_level_catalog(const std::string& catalog_path, int level) {
  if (catalog_path.empty()) {
    throw std::invalid_argument("Adaptive catalog path is empty");
  }

  std::filesystem::path path{catalog_path};
  if (!path.is_absolute()) {
    path = std::filesystem::current_path() / path;
  }
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
    throw std::runtime_error("Adaptive catalog not found at: " + path.string());
  }

  if (path.extension() != ".json") {
    throw std::runtime_error("Adaptive catalog must be provided as JSON: " + path.string());
  }

  std::ifstream stream(path);
  if (!stream) {
    throw std::runtime_error("Failed to open adaptive catalog: " + path.string());
  }
  std::string content((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
  auto document = nlohmann::json::parse(content);
  auto all_specs = DrillSpec::load_json(document);
  auto filtered = DrillSpec::filter_by_level(all_specs, level);
  if (filtered.empty()) {
    throw std::runtime_error("No adaptive drills configured for level " + std::to_string(level));
  }
  return filtered;
}

} // namespace ear::adaptive
