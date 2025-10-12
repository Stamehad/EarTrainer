#include "../include/ear/adaptive_catalog.hpp"

#include <filesystem>
#include <stdexcept>

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
    throw std::runtime_error("Adaptive catalog not found at: " + path.string());
  }

  auto all_specs = DrillSpec::load_yaml(path.string());
  auto filtered = DrillSpec::filter_by_level(all_specs, level);
  if (filtered.empty()) {
    throw std::runtime_error("No adaptive drills configured for level " + std::to_string(level));
  }
  return filtered;
}

} // namespace ear::adaptive
