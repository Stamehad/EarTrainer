#pragma once

#include "drill_factory.hpp"
#include "types.hpp"
#include "../resources/catalog_manager2.hpp"

#include <optional>
#include <string>
#include <vector>
#include <filesystem>
#include <unordered_map>
#include <map>

namespace ear {

class LevelInspector {
public:
  LevelInspector(std::filesystem::path resources_dir,
                 std::string catalog_basename,
                 std::uint64_t seed = 1);

  void set_base_spec(const SessionSpec& spec);

  std::string overview() const;
  std::string levels_summary() const;
  std::vector<int> known_levels() const;
  std::vector<int> tiers_for_level(int level) const;
  std::vector<LevelCatalogEntry> catalog_entries() const;

  void select(int level, int tier);
  bool has_selection() const;
  std::optional<std::pair<int, int>> selection() const;

  QuestionBundle next();

private:
  struct Slot {
    std::string id;
    DrillSpec spec;
    std::unique_ptr<DrillModule> module;
    std::uint64_t rng_state = 0;
  };

  void load_catalog();
  std::string make_question_id();
  const resources::ManifestView::Lesson* lesson_for(int level) const;
  std::map<int, std::vector<DrillSpec>> describe_level_specs(int level) const;

  std::string catalog_basename_;
  std::string catalog_display_name_;
  std::vector<int> levels_;
  std::vector<std::string> allowed_catalogs_;
  std::vector<Slot> slots_;
  std::uint64_t master_rng_;
  std::uint64_t question_counter_ = 0;
  std::size_t next_slot_index_ = 0;
  std::optional<int> active_level_;
  std::optional<int> active_tier_;
  DrillFactory& factory_;
  std::optional<std::string> base_key_;
  resources::ManifestView manifest_;
  std::unordered_map<int, const resources::ManifestView::Lesson*> lesson_lookup_;
};

} // namespace ear
