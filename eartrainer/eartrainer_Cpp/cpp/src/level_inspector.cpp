#include "ear/level_inspector.hpp"

#include "ear/question_bundle_v2.hpp"
#include "resources/catalog_manager.hpp"
#include "rng.hpp"

#include <algorithm>
#include <iomanip>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <cctype>

namespace ear {
namespace {

void ensure_factory_registered(DrillFactory& factory) {
  static std::once_flag flag;
  std::call_once(flag, [&factory]() { register_builtin_drills(factory); });
}

std::string to_lower(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (unsigned char ch : text) {
    out.push_back(static_cast<char>(std::tolower(ch)));
  }
  return out;
}

bool has_catalog(const resources::CatalogIndex& index, std::string_view name) {
  for (const auto& track : index.tracks()) {
    if (track.name == name) {
      return true;
    }
  }
  return false;
}

std::optional<std::string> resolve_catalog_name(const resources::CatalogIndex& index,
                                                const std::string& key) {
  const std::string lower = to_lower(key);
  for (const auto& track : index.tracks()) {
    if (lower == to_lower(track.name)) {
      return track.name;
    }
  }

  if ((lower == "degree" || lower == "degrees") && has_catalog(index, "degree_levels")) {
    return std::string("degree_levels");
  }
  if ((lower == "melody" || lower == "melodies") && has_catalog(index, "melody_levels")) {
    return std::string("melody_levels");
  }
  if ((lower == "chord" || lower == "chords" || lower == "chord_sustain") &&
      has_catalog(index, "chord_levels")) {
    return std::string("chord_levels");
  }
  return std::nullopt;
}

std::vector<std::string> all_catalog_names(const resources::CatalogIndex& index) {
  std::vector<std::string> names;
  names.reserve(index.track_count());
  for (const auto& track : index.tracks()) {
    names.push_back(track.name);
  }
  return names;
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

void LevelInspector::set_base_spec(const SessionSpec& spec) {
  base_key_ = spec.key;
  for (auto& slot : slots_) {
    slot.spec.key = spec.key;
  }
}

void LevelInspector::load_catalog() {
  slots_.clear();
  question_counter_ = 0;
  next_slot_index_ = 0;
  active_level_.reset();
  active_tier_.reset();

  allowed_catalogs_.clear();
  levels_.clear();

  catalog_index_ = resources::build_builtin_catalog_index();

  const bool load_all = catalog_basename_.empty() || catalog_basename_ == "all" ||
                        catalog_basename_ == "builtin" ||
                        catalog_basename_ == "all_builtin";

  if (load_all) {
    catalog_display_name_ = "builtin";
    allowed_catalogs_ = all_catalog_names(catalog_index_);
  } else {
    auto resolved = resolve_catalog_name(catalog_index_, catalog_basename_);
    if (!resolved.has_value()) {
      throw std::runtime_error("LevelInspector: unknown builtin catalog '" + catalog_basename_ + "'");
    }
    catalog_display_name_ = *resolved;
    allowed_catalogs_.push_back(*resolved);
  }

  for (const auto& entry : catalog_index_.entries()) {
    if (std::find(allowed_catalogs_.begin(), allowed_catalogs_.end(), entry.catalog) ==
        allowed_catalogs_.end()) {
      continue;
    }
    levels_.push_back(entry.level);
  }

  std::sort(levels_.begin(), levels_.end());
  levels_.erase(std::unique(levels_.begin(), levels_.end()), levels_.end());

  if (levels_.empty()) {
    throw std::runtime_error("LevelInspector: catalog '" + catalog_display_name_ + "' is empty");
  }
}

std::string LevelInspector::overview() const {
  std::ostringstream oss;
  oss << catalog_display_name_ << " levels\n";
  const auto descriptions = catalog_index_.describe_levels(levels_);
  for (const auto& desc : descriptions) {
    oss << "  Level " << desc.level << ": ";
    if (desc.tiers.empty()) {
      oss << "(no drills)\n";
      continue;
    }
    bool first_tier = true;
    for (const auto& [tier, specs] : desc.tiers) {
      if (!first_tier) {
        oss << " | ";
      }
      first_tier = false;
      oss << "tier " << tier << " -> [";
      bool first_id = true;
      for (const DrillSpec* spec : specs) {
        if (!spec) {
          continue;
        }
        if (!first_id) {
          oss << ", ";
        }
        first_id = false;
        oss << spec->id;
      }
      oss << "]";
    }
    oss << "\n";
  }
  return oss.str();
}

std::string LevelInspector::levels_summary() const {
  if (levels_.empty()) {
    return "Levels: (none)";
  }
  std::ostringstream oss;
  oss << "Levels: ";
  bool first_level = true;
  const auto descriptions = catalog_index_.describe_levels(levels_);
  for (const auto& desc : descriptions) {
    if (!first_level) {
      oss << ", ";
    }
    first_level = false;
    oss << desc.level << " (";
    bool first_tier = true;
    for (const auto& [tier, _] : desc.tiers) {
      if (!first_tier) {
        oss << ",";
      }
      first_tier = false;
      oss << tier;
    }
    oss << ")";
  }
  return oss.str();
}

std::vector<int> LevelInspector::known_levels() const {
  return levels_;
}

std::vector<int> LevelInspector::tiers_for_level(int level) const {
  std::vector<int> tiers;
  if (!std::binary_search(levels_.begin(), levels_.end(), level)) {
    return tiers;
  }
  const auto desc = catalog_index_.describe_level(level);
  tiers.reserve(desc.tiers.size());
  for (const auto& [tier, _] : desc.tiers) {
    tiers.push_back(tier);
  }
  return tiers;
}

std::vector<LevelCatalogEntry> LevelInspector::catalog_entries() const {
  std::vector<LevelCatalogEntry> entries;
  if (levels_.empty()) {
    return entries;
  }

  const auto descriptions = catalog_index_.describe_levels(levels_);
  for (const auto& desc : descriptions) {
    for (const auto& [tier, specs] : desc.tiers) {
      if (specs.empty() || specs.front() == nullptr) {
        continue;
      }
      std::ostringstream oss;
      oss << desc.level << "-" << tier << ": " << specs.front()->id;
      LevelCatalogEntry entry;
      entry.level = desc.level;
      entry.tier = tier;
      entry.label = oss.str();
      entries.push_back(std::move(entry));
    }
  }
  return entries;
}

void LevelInspector::select(int level, int tier) {
  if (!std::binary_search(levels_.begin(), levels_.end(), level)) {
    throw std::runtime_error("LevelInspector: unknown level " + std::to_string(level));
  }

  const auto desc = catalog_index_.describe_level(level);
  auto tier_it = desc.tiers.find(tier);
  if (tier_it == desc.tiers.end() || tier_it->second.empty()) {
    throw std::runtime_error("LevelInspector: no drills for level " + std::to_string(level) +
                             ", tier " + std::to_string(tier));
  }

  std::vector<DrillSpec> selected_specs;
  selected_specs.reserve(tier_it->second.size());
  for (const DrillSpec* spec_ptr : tier_it->second) {
    if (!spec_ptr) {
      continue;
    }
    DrillSpec copy = *spec_ptr;
    if (base_key_.has_value()) {
      copy.key = *base_key_;
    }
    selected_specs.push_back(std::move(copy));
  }

  if (selected_specs.empty()) {
    throw std::runtime_error("LevelInspector: tier " + std::to_string(tier) +
                             " for level " + std::to_string(level) + " has no drills");
  }

  slots_.clear();
  question_counter_ = 0;
  next_slot_index_ = 0;
  active_level_ = level;
  active_tier_ = tier;

  std::uint64_t seed = master_rng_;
  for (const auto& spec : selected_specs) {
    auto assignment = factory_.create(spec);
    Slot slot;
    slot.id = assignment.id;
    slot.spec = assignment.spec;
    if (base_key_.has_value()) {
      slot.spec.key = *base_key_;
    }
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
  // apply_prompt_rendering(slot.spec, bundle);
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
