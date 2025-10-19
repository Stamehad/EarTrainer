#pragma once

#include "types.hpp"
#include "drill_factory.hpp"

#include <map>
#include <optional>
#include <string>
#include <vector>
#include <filesystem>

namespace ear {

class LevelInspector {
public:
  LevelInspector(std::filesystem::path resources_dir,
                 std::string catalog_basename,
                 std::uint64_t seed = 1);

  std::string overview() const;
  std::string levels_summary() const;
  std::vector<int> known_levels() const;
  std::vector<int> tiers_for_level(int level) const;

  void select(int level, int tier);
  bool has_selection() const;
  std::optional<std::pair<int, int>> selection() const;

  QuestionBundle next();

private:
public:
  struct DrillEntry {
    DrillSpec spec;
    int tier = 0;
  };

private:
  struct Slot {
    std::string id;
    DrillSpec spec;
    std::unique_ptr<DrillModule> module;
    std::uint64_t rng_state = 0;
  };

  void load_catalog();
  std::string make_question_id();

  std::string catalog_basename_;
  std::string catalog_display_name_;
  std::vector<DrillEntry> entries_;
  std::map<int, std::map<int, std::vector<std::size_t>>> index_;
  std::vector<Slot> slots_;
  std::uint64_t master_rng_;
  std::uint64_t question_counter_ = 0;
  std::size_t next_slot_index_ = 0;
  std::optional<int> active_level_;
  std::optional<int> active_tier_;
  DrillFactory& factory_;
};

} // namespace ear
