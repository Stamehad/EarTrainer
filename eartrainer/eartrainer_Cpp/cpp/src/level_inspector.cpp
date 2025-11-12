#include "ear/level_inspector.hpp"

#include "ear/question_bundle_v2.hpp"
#include "rng.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace ear {
namespace {

using Lesson = resources::ManifestView::Lesson;
using DrillEntry = ear::builtin::catalog_numbered::DrillEntry;

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

struct TrackCatalog {
  std::string name;
  const std::vector<Lesson>* lessons = nullptr;
};

std::vector<TrackCatalog> manifest_tracks(const resources::ManifestView& manifest) {
  return {
      {resources::ManifestView::kTrackNames[0], &manifest.melody},
      {resources::ManifestView::kTrackNames[1], &manifest.harmony},
      {resources::ManifestView::kTrackNames[2], &manifest.chords},
  };
}

bool has_catalog(const std::vector<TrackCatalog>& tracks, std::string_view name) {
  const auto lower = to_lower(name);
  return std::any_of(tracks.begin(), tracks.end(), [&](const TrackCatalog& track) {
    return to_lower(track.name) == lower;
  });
}

std::vector<std::string> all_catalog_names(const std::vector<TrackCatalog>& tracks) {
  std::vector<std::string> names;
  names.reserve(tracks.size());
  for (const auto& track : tracks) {
    names.push_back(track.name);
  }
  return names;
}

std::optional<std::string> resolve_catalog_name(const std::vector<TrackCatalog>& tracks,
                                                const std::string& key) {
  const std::string lower = to_lower(key);
  for (const auto& track : tracks) {
    if (lower == to_lower(track.name)) {
      return track.name;
    }
  }

  struct Alias {
    std::string alias;
    std::string canonical;
  };
  static const Alias kAliases[] = {
      {"degree", "harmony"},
      {"degrees", "harmony"},
      {"degree_levels", "harmony"},
      {"harmony_levels", "harmony"},
      {"melodies", "melody"},
      {"melody_levels", "melody"},
      {"chords", "chord"},
      {"chord_levels", "chord"},
      {"chord_sustain", "chord"},
  };
  for (const auto& alias : kAliases) {
    if (lower == alias.alias && has_catalog(tracks, alias.canonical)) {
      return alias.canonical;
    }
  }

  return std::nullopt;
}

DrillSpec make_spec_from_entry(const Lesson& lesson,
                               const DrillEntry& drill,
                               int ordinal) {
  DrillSpec spec;
  DrillParams params = drill.build ? drill.build() : DrillParams{};
  spec.id = drill.name
                ? std::string(lesson.name) + "#" + std::to_string(ordinal)
                : std::to_string(drill.number) + "#" + std::to_string(ordinal);
  spec.family = ear::builtin::catalog_numbered::family_of(params);
  spec.level = ear::builtin::catalog_numbered::block_of(drill.number);
  spec.tier = ear::builtin::catalog_numbered::tier_of(drill.number);
  spec.params = std::move(params);
  return spec;
}

} // namespace

LevelInspector::LevelInspector(std::filesystem::path resources_dir,
                               std::string catalog_basename,
                               std::uint64_t seed)
    : catalog_basename_(std::move(catalog_basename)),
      master_rng_(seed == 0 ? 1 : seed),
      factory_(DrillFactory::instance()),
      manifest_(resources::manifest()) {
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
  lesson_lookup_.clear();

  const auto tracks = manifest_tracks(manifest_);
  const bool load_all = catalog_basename_.empty() || catalog_basename_ == "all" ||
                        catalog_basename_ == "builtin" ||
                        catalog_basename_ == "all_builtin";

  if (load_all) {
    catalog_display_name_ = "builtin";
    allowed_catalogs_ = all_catalog_names(tracks);
  } else {
    auto resolved = resolve_catalog_name(tracks, catalog_basename_);
    if (!resolved.has_value()) {
      throw std::runtime_error("LevelInspector: unknown catalog '" + catalog_basename_ + "'");
    }
    catalog_display_name_ = *resolved;
    allowed_catalogs_.push_back(*resolved);
  }

  for (const auto& track : tracks) {
    if (std::find(allowed_catalogs_.begin(), allowed_catalogs_.end(), track.name) ==
        allowed_catalogs_.end()) {
      continue;
    }
    if (!track.lessons) {
      continue;
    }
    for (const auto& lesson : *track.lessons) {
      levels_.push_back(lesson.lesson);
      lesson_lookup_[lesson.lesson] = &lesson;
    }
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
  for (int level : levels_) {
    const auto tier_map = describe_level_specs(level);
    oss << "  Level " << level << ": ";
    if (tier_map.empty()) {
      oss << "(no drills)\n";
      continue;
    }
    bool first_tier = true;
    for (const auto& [tier, specs] : tier_map) {
      if (!first_tier) {
        oss << " | ";
      }
      first_tier = false;
      oss << "tier " << tier << " -> [";
      bool first_id = true;
      for (const auto& spec : specs) {
        if (!first_id) {
          oss << ", ";
        }
        first_id = false;
        oss << spec.id;
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
  for (int level : levels_) {
    if (!first_level) {
      oss << ", ";
    }
    first_level = false;
    const auto tier_map = describe_level_specs(level);
    oss << level << " (";
    bool first_tier = true;
    for (const auto& [tier, _] : tier_map) {
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
  const auto tier_map = describe_level_specs(level);
  tiers.reserve(tier_map.size());
  for (const auto& [tier, _] : tier_map) {
    tiers.push_back(tier);
  }
  return tiers;
}

std::vector<LevelCatalogEntry> LevelInspector::catalog_entries() const {
  std::vector<LevelCatalogEntry> entries;
  for (int level : levels_) {
    const auto tier_map = describe_level_specs(level);
    for (const auto& [tier, specs] : tier_map) {
      if (specs.empty()) {
        continue;
      }
      std::ostringstream oss;
      oss << level << "-" << tier << ": " << specs.front().id;
      LevelCatalogEntry entry;
      entry.level = level;
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

  auto tier_map = describe_level_specs(level);
  auto tier_it = tier_map.find(tier);
  if (tier_it == tier_map.end() || tier_it->second.empty()) {
    throw std::runtime_error("LevelInspector: no drills for level " + std::to_string(level) +
                             ", tier " + std::to_string(tier));
  }

  std::vector<DrillSpec> selected_specs = tier_it->second;
  if (selected_specs.empty()) {
    throw std::runtime_error("LevelInspector: tier " + std::to_string(tier) +
                             " for level " + std::to_string(level) + " has no drills");
  }
  if (base_key_.has_value()) {
    for (auto& spec : selected_specs) {
      spec.key = *base_key_;
    }
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

const Lesson* LevelInspector::lesson_for(int level) const {
  const auto it = lesson_lookup_.find(level);
  if (it == lesson_lookup_.end()) {
    return nullptr;
  }
  return it->second;
}

std::map<int, std::vector<DrillSpec>> LevelInspector::describe_level_specs(int level) const {
  std::map<int, std::vector<DrillSpec>> tiers;
  const auto* lesson = lesson_for(level);
  if (!lesson) {
    return tiers;
  }
  int ordinal = 0;
  for (const auto& drill : lesson->drills) {
    if (!drill.build) {
      continue;
    }
    auto spec = make_spec_from_entry(*lesson, drill, ordinal++);
    const int tier_key = spec.tier.has_value() ? *spec.tier : -1;
    tiers[tier_key].push_back(std::move(spec));
  }
  return tiers;
}

} // namespace ear
