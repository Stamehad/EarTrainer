#include "ear/level_inspector.hpp"

#include "rng.hpp"
#include "resources/builtin_degree_levels.hpp"
#include "resources/builtin_melody_levels.hpp"
#include "resources/builtin_chord_levels.hpp"

#include <algorithm>
#include <array>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace ear {
namespace {

void ensure_factory_registered(DrillFactory& factory) {
  static std::once_flag flag;
  std::call_once(flag, [&factory]() { register_builtin_drills(factory); });
}

struct CatalogAccess {
  std::string_view canonical;
  const std::vector<int>& (*known_levels)();
  const std::vector<DrillSpec>& (*drills_for_level)(int);
};

std::string normalize_catalog_key(const std::string& key) {
  if (key == "degree") return "degree_levels";
  if (key == "melody") return "melody_levels";
  if (key == "chord") return "chord_levels";
  return key;
}

const CatalogAccess& resolve_catalog(const std::string& key) {
  static const std::array<CatalogAccess, 3> catalogs = {{
      {ear::builtin::DegreeLevels::name,
       ear::builtin::DegreeLevels::known_levels,
       ear::builtin::DegreeLevels::drills_for_level},
      {ear::builtin::MelodyLevels::name,
       ear::builtin::MelodyLevels::known_levels,
       ear::builtin::MelodyLevels::drills_for_level},
      {ear::builtin::ChordLevels::name,
       ear::builtin::ChordLevels::known_levels,
       ear::builtin::ChordLevels::drills_for_level},
  }};
  const std::string normalized = normalize_catalog_key(key);
  for (const auto& catalog : catalogs) {
    if (normalized == catalog.canonical) {
      return catalog;
    }
  }
  throw std::runtime_error("LevelInspector: unknown builtin catalog '" + key + "'");
}

} // namespace

LevelInspector::LevelInspector(std::filesystem::path resources_dir,
                               std::string catalog_basename,
                               std::uint64_t seed)
    : catalog_basename_(std::move(catalog_basename)),
      master_rng_(seed == 0 ? 1 : seed),
      factory_(DrillFactory::instance()) {
  (void)resources_dir;
  ensure_factory_registered(factory_);
  load_catalog();
}

void LevelInspector::load_catalog() {
  entries_.clear();
  index_.clear();

  const auto& catalog = resolve_catalog(catalog_basename_);
  catalog_basename_ = normalize_catalog_key(catalog_basename_);
  catalog_display_name_ = std::string(catalog.canonical);

  for (int level : catalog.known_levels()) {
    const auto& drills = catalog.drills_for_level(level);
    if (drills.empty()) {
      continue;
    }
    for (std::size_t i = 0; i < drills.size(); ++i) {
      DrillEntry drill_entry;
      drill_entry.spec = drills[i];
      drill_entry.tier = static_cast<int>(i);
      const std::size_t idx = entries_.size();
      entries_.push_back(drill_entry);
      index_[level][drill_entry.tier].push_back(idx);
    }
  }

  for (auto& [level, tiers] : index_) {
    for (auto& [tier, indices] : tiers) {
      std::sort(indices.begin(), indices.end(),
                [&](std::size_t a, std::size_t b) {
                  return entries_[a].spec.id < entries_[b].spec.id;
                });
    }
  }

  if (entries_.empty()) {
    throw std::runtime_error("LevelInspector: builtin catalog '" + catalog_display_name_ + "' is empty");
  }
}

std::string LevelInspector::overview() const {
  std::ostringstream oss;
  oss << catalog_display_name_ << " levels\n";
  for (const auto& [level, tiers] : index_) {
    oss << "  Level " << level << ": ";
    bool first_tier = true;
    for (const auto& [tier, indices] : tiers) {
      if (!first_tier) {
        oss << " | ";
      }
      first_tier = false;
      oss << "tier " << tier << " -> [";
      bool first_id = true;
      for (std::size_t idx : indices) {
        if (!first_id) {
          oss << ", ";
        }
        first_id = false;
        oss << entries_[idx].spec.id;
      }
      oss << "]";
    }
    oss << "\n";
  }
  return oss.str();
}

std::vector<int> LevelInspector::known_levels() const {
  std::vector<int> levels;
  levels.reserve(index_.size());
  for (const auto& [level, _] : index_) {
    levels.push_back(level);
  }
  return levels;
}

std::vector<int> LevelInspector::tiers_for_level(int level) const {
  std::vector<int> tiers;
  auto it = index_.find(level);
  if (it == index_.end()) {
    return tiers;
  }
  tiers.reserve(it->second.size());
  for (const auto& [tier, _] : it->second) {
    tiers.push_back(tier);
  }
  return tiers;
}

void LevelInspector::select(int level, int tier) {
  auto level_it = index_.find(level);
  if (level_it == index_.end()) {
    throw std::runtime_error("LevelInspector: unknown level " + std::to_string(level));
  }
  auto tier_it = level_it->second.find(tier);
  if (tier_it == level_it->second.end() || tier_it->second.empty()) {
    throw std::runtime_error("LevelInspector: no drills for level " + std::to_string(level) +
                             ", tier " + std::to_string(tier));
  }

  slots_.clear();
  question_counter_ = 0;
  next_slot_index_ = 0;
  active_level_ = level;
  active_tier_ = tier;

  std::uint64_t seed = master_rng_;
  for (std::size_t idx : tier_it->second) {
    const auto& entry = entries_[idx];
    auto assignment = factory_.create(entry.spec);
    Slot slot;
    slot.id = assignment.id;
    slot.spec = assignment.spec;
    slot.module = std::move(assignment.module);
    slot.rng_state = advance_rng(seed);
    slots_.push_back(std::move(slot));
  }
  master_rng_ = seed;

  if (slots_.empty()) {
    throw std::runtime_error("LevelInspector: selection produced zero drills");
  }
}

bool LevelInspector::has_selection() const {
  return active_level_.has_value() && active_tier_.has_value() && !slots_.empty();
}

std::optional<std::pair<int, int>> LevelInspector::selection() const {
  if (!has_selection()) {
    return std::nullopt;
  }
  return std::make_pair(active_level_.value(), active_tier_.value());
}

QuestionBundle LevelInspector::next() {
  if (!has_selection()) {
    throw std::runtime_error("LevelInspector: select a level/tier before requesting questions");
  }
  if (next_slot_index_ >= slots_.size()) {
    next_slot_index_ = 0;
  }
  auto& slot = slots_[next_slot_index_];
  auto bundle = slot.module->next_question(slot.rng_state);
  apply_prompt_rendering(slot.spec, bundle);
  bundle.question_id = make_question_id();
  next_slot_index_ = (next_slot_index_ + 1) % slots_.size();
  return bundle;
}

std::string LevelInspector::make_question_id() {
  std::ostringstream oss;
  oss << "li-";
  oss.width(3);
  oss.fill('0');
  oss << ++question_counter_;
  return oss.str();
}

} // namespace ear
