#include "resources/catalog_manager.hpp"

#include "resources/builtin_chord_levels.hpp"
#include "resources/builtin_degree_levels.hpp"
#include "resources/builtin_melody_levels.hpp"
#include "rng.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

namespace ear::resources {

const DrillSpec* CatalogIndex::Entry::drill_at(int tier) const {
  if (tier < 0) {
    return nullptr;
  }
  const auto& specs = drills();
  const auto idx = static_cast<std::size_t>(tier);
  if (idx >= specs.size()) {
    return nullptr;
  }
  return &specs[idx];
}

CatalogIndex::CatalogIndex() = default;

CatalogIndex::CatalogIndex(std::vector<Entry> entries, std::vector<TrackInfo> tracks)
    : index_(std::move(entries)), tracks_(std::move(tracks)) {
  std::sort(index_.begin(), index_.end(),
            [](const Entry& lhs, const Entry& rhs) {
              if (lhs.catalog == rhs.catalog) {
                return lhs.level < rhs.level;
              }
              return lhs.catalog < rhs.catalog;
            });
  build_level_lookup();
}

const std::string& CatalogIndex::track_name(std::size_t idx) const {
  if (idx >= tracks_.size()) {
    throw std::out_of_range("CatalogIndex: track index out of range");
  }
  return tracks_[idx].name;
}

const CatalogIndex::Entry* CatalogIndex::entry_for_level(int level) const {
  const auto it = level_lookup_.find(level);
  if (it == level_lookup_.end()) {
    return nullptr;
  }
  return &index_[it->second];
}

const CatalogIndex::Entry* CatalogIndex::find_level(int level) const {
  return entry_for_level(level);
}

const CatalogIndex::Entry* CatalogIndex::find_level(std::string_view catalog, int level) const {
  const auto* entry = entry_for_level(level);
  if (!entry || entry->catalog != catalog) {
    return nullptr;
  }
  return entry;
}

const DrillSpec* CatalogIndex::drill_for(int level, int tier) const {
  const auto* entry = entry_for_level(level);
  if (!entry) {
    return nullptr;
  }
  return entry->drill_at(tier);
}

std::vector<int> CatalogIndex::known_levels() const {
  std::vector<int> levels;
  levels.reserve(index_.size());
  for (const auto& entry : index_) {
    levels.push_back(entry.level);
  }
  std::sort(levels.begin(), levels.end());
  levels.erase(std::unique(levels.begin(), levels.end()), levels.end());
  return levels;
}

std::vector<int> CatalogIndex::known_levels(std::string_view catalog) const {
  std::vector<int> levels;
  for (const auto& entry : index_) {
    if (entry.catalog == catalog) {
      levels.push_back(entry.level);
    }
  }
  std::sort(levels.begin(), levels.end());
  levels.erase(std::unique(levels.begin(), levels.end()), levels.end());
  return levels;
}

const std::vector<DrillSpec>& CatalogIndex::drills_for_level(int level) const {
  const auto* entry = entry_for_level(level);
  if (!entry) {
    throw std::out_of_range("CatalogIndex: unknown level " + std::to_string(level));
  }
  return entry->drills();
}

const std::vector<DrillSpec>* CatalogIndex::try_drills_for_level(int level) const {
  const auto* entry = entry_for_level(level);
  if (!entry) {
    return nullptr;
  }
  return &entry->drills();
}

void CatalogIndex::build_level_lookup() {
  level_lookup_.clear();
  level_lookup_.reserve(index_.size());
  for (std::size_t i = 0; i < index_.size(); ++i) {
    const auto& entry = index_[i];
    if (!entry.generator) {
      throw std::logic_error("CatalogIndex entry missing generator");
    }
    const auto [it, inserted] = level_lookup_.emplace(entry.level, i);
    if (!inserted) {
      throw std::logic_error("CatalogIndex duplicate level: " + std::to_string(entry.level));
    }
  }
}

int CatalogIndex::first_level_for_track(std::size_t track_index) const {
  if (track_index >= tracks_.size()) {
    return 0;
  }
  const auto& phases = tracks_[track_index].phases;
  for (const auto& [phase, levels] : phases) {
    (void)phase;
    if (!levels.empty()) {
      return levels.front();
    }
  }
  return 0;
}

std::vector<int> CatalogIndex::levels_in_scope(std::size_t track_index,
                                               int current_level,
                                               int phase_digit) const {
  std::vector<int> out;
  if (track_index >= tracks_.size()) {
    return out;
  }
  const auto& phases = tracks_[track_index].phases;
  const auto it = phases.find(phase_digit);
  if (it == phases.end()) {
    return out;
  }
  const auto& phase_levels = it->second;
  const int level_phase = (std::abs(current_level) / 10) % 10;
  if (level_phase < phase_digit) {
    out = phase_levels;
  } else if (level_phase == phase_digit) {
    const auto lb = std::lower_bound(phase_levels.begin(), phase_levels.end(), current_level);
    out.assign(lb, phase_levels.end());
  }
  return out;
}

int CatalogIndex::weighted_pick(const std::vector<int>& weights, std::uint64_t& rng_state) {
  long long total = 0;
  for (int w : weights) {
    total += (w > 0 ? w : 0);
  }
  if (total <= 0) {
    return -1;
  }
  long long r = rand_int(rng_state, 1, static_cast<int>(total));
  long long acc = 0;
  for (std::size_t i = 0; i < weights.size(); ++i) {
    if (weights[i] <= 0) {
      continue;
    }
    acc += weights[i];
    if (r <= acc) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

CatalogIndex::TrackLevelPick CatalogIndex::pick_next_level(
    const std::vector<int>& current_levels,
    std::uint64_t& rng_state) const {
  TrackLevelPick pick;
  if (tracks_.empty()) {
    return pick;
  }

  //-------------------------------------------------------------------------------
  // GUARD: IF LEVELS ARE MISSING OR INVALID, FILL WITH FIRST LEVELS OF EACH TRACK
  //-------------------------------------------------------------------------------
  const std::size_t track_count = tracks_.size();
  std::vector<int> normalized = current_levels;
  if (normalized.size() < track_count) {
    normalized.resize(track_count, 0);
  } else if (normalized.size() > track_count) {
    normalized.resize(track_count);
  }

  for (std::size_t i = 0; i < track_count; ++i) {
    if (normalized[i] <= 0) {
      const int fallback = first_level_for_track(i);
      if (fallback > 0) {
        normalized[i] = fallback;
      }
    }
  }
  pick.normalized_levels = normalized;

  //-------------------------------------------------------------------------------
  // FIND CURENT PHASE
  //-------------------------------------------------------------------------------
  std::optional<int> active_phase;
  for (int level : normalized) {
    if (level <= 0) {
      continue;
    }
    const int phase = (std::abs(level) / 10) % 10;
    if (!active_phase || phase < *active_phase) {
      active_phase = phase;
    }
  }

  if (!active_phase.has_value()) {
    return pick;
  }
  pick.phase_digit = *active_phase;

  //-------------------------------------------------------------------------------
  // COMPUTE WEIGHTS AND PICK TRACK
  //-------------------------------------------------------------------------------
  pick.weights.assign(track_count, 0);
  for (std::size_t i = 0; i < track_count; ++i) {
    const int level = normalized[i];
    if (level <= 0) {
      continue;
    }

    const auto& track = tracks_[i];
    const int level_phase = (std::abs(level) / 10) % 10;
    if (level_phase > pick.phase_digit) {
      continue;
    }

    const auto phase_it = track.phases.find(pick.phase_digit);
    if (phase_it == track.phases.end() || phase_it->second.empty()) {
      if (level_phase == pick.phase_digit) {
        throw std::runtime_error(
            "CatalogIndex: no levels found in active phase for track '" + track.name + "'");
      }
      continue;
    }

    const auto& phase_levels = phase_it->second;
    if (level_phase < pick.phase_digit) {
      pick.weights[i] = static_cast<int>(phase_levels.size());
      continue;
    }

    const auto it = std::lower_bound(phase_levels.begin(), phase_levels.end(), level);
    if (it == phase_levels.end() || *it != level) {
      throw std::runtime_error(
          "CatalogIndex: current level " + std::to_string(level) +
          " not found in track '" + track.name + "'");
    }
    pick.weights[i] = static_cast<int>(phase_levels.end() - it);
  }

  pick.track_index = weighted_pick(pick.weights, rng_state);
  if (pick.track_index < 0) {
    return pick;
  }

  const auto scope = levels_in_scope(static_cast<std::size_t>(pick.track_index),
                                     pick.normalized_levels[static_cast<std::size_t>(pick.track_index)],
                                     pick.phase_digit);
  if (scope.empty()) {
    throw std::runtime_error(
        "CatalogIndex: selected track '" + tracks_[static_cast<std::size_t>(pick.track_index)].name +
        "' has no pending levels in phase " + std::to_string(pick.phase_digit));
  }
  pick.level = scope.front();
  return pick;
}

CatalogIndex::LevelDescription CatalogIndex::describe_level(int level) const {
  LevelDescription desc;
  desc.level = level;
  const auto* entry = entry_for_level(level);
  if (!entry) {
    return desc;
  }
  const auto& drills = entry->drills();
  for (std::size_t i = 0; i < drills.size(); ++i) {
    const auto& spec = drills[i];
    const int tier = spec.tier.has_value() ? *spec.tier : static_cast<int>(i);
    desc.tiers[tier].push_back(&spec);
  }
  for (auto& [tier, specs] : desc.tiers) {
    (void)tier;
    std::sort(specs.begin(), specs.end(), [](const DrillSpec* a, const DrillSpec* b) {
      if (a == nullptr || b == nullptr) {
        return a < b;
      }
      return a->id < b->id;
    });
  }
  return desc;
}

std::vector<CatalogIndex::LevelDescription> CatalogIndex::describe_levels(const std::vector<int>& levels) const {
  std::vector<LevelDescription> description;
  description.reserve(levels.size());
  for (int level : levels) {
    description.push_back(describe_level(level));
  }
  return description;
}

CatalogIndex build_builtin_catalog_index() {
  std::vector<CatalogIndex::Entry> entries;
  std::vector<CatalogIndex::TrackInfo> tracks;

  entries.reserve(
      ear::builtin::DegreeLevels::detail::generator_table().size() +
      ear::builtin::MelodyLevels::detail::generator_table().size() +
      ear::builtin::ChordLevels::detail::generator_table().size());
  tracks.reserve(3);

  const auto append = [&](std::string_view catalog,
                          const auto& generator_rows) {
    CatalogIndex::TrackInfo track;
    track.name = std::string(catalog);
    for (const auto& row : generator_rows) {
      const int phase = (std::abs(row.level) / 10) % 10;
      track.phases[phase].push_back(row.level);
      entries.push_back(
          CatalogIndex::Entry{std::string(catalog), row.level, row.generator});
    }
    for (auto& [phase, levels] : track.phases) {
      std::sort(levels.begin(), levels.end());
    }
    tracks.push_back(std::move(track));
  };

  append(ear::builtin::DegreeLevels::name,
         ear::builtin::DegreeLevels::detail::generator_table());
  append(ear::builtin::MelodyLevels::name,
         ear::builtin::MelodyLevels::detail::generator_table());
  append(ear::builtin::ChordLevels::name,
         ear::builtin::ChordLevels::detail::generator_table());

  return CatalogIndex{std::move(entries), std::move(tracks)};
}

} // namespace ear::resources
