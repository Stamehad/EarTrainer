#pragma once

#include "ear/drill_spec.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ear::resources {

/**
 * CatalogIndex consolidates the lazily-evaluated level catalogs across drill
 * families and provides track-aware helpers for adaptive selection.
 */
class CatalogIndex {
public:
  using DrillGenerator = const std::vector<DrillSpec>& (*)();

  struct TrackInfo {
    std::string name;
    std::map<int, std::vector<int>> phases; // phase_digit -> sorted levels
  };

  struct Entry {
    std::string catalog;
    int level = 0;
    DrillGenerator generator = nullptr;

    const std::vector<DrillSpec>& drills() const {
      if (!generator) {
        throw std::logic_error("CatalogIndex::Entry has no generator");
      }
      return generator();
    }

    const DrillSpec* drill_at(int tier) const;
  };

  struct TrackLevelPick {
    int track_index = -1;
    int level = 0;
    int phase_digit = -1;
    std::vector<int> weights;
    std::vector<int> normalized_levels;

    bool valid() const { return track_index >= 0 && level > 0; }
  };

  struct LevelDescription {
    int level = 0;
    std::map<int, std::vector<const DrillSpec*>> tiers;
  };

  CatalogIndex();
  CatalogIndex(std::vector<Entry> entries, std::vector<TrackInfo> tracks);

  bool empty() const { return index_.empty(); }
  std::size_t size() const { return index_.size(); }

  const std::vector<Entry>& entries() const { return index_; }
  const std::vector<TrackInfo>& tracks() const { return tracks_; }
  std::size_t track_count() const { return tracks_.size(); }
  const std::string& track_name(std::size_t idx) const;

  const Entry* find_level(int level) const;
  const Entry* find_level(std::string_view catalog, int level) const;

  const DrillSpec* drill_for(int level, int tier) const;

  std::vector<int> known_levels() const;
  std::vector<int> known_levels(std::string_view catalog) const;

  const std::vector<DrillSpec>& drills_for_level(int level) const;
  const std::vector<DrillSpec>* try_drills_for_level(int level) const;

  TrackLevelPick pick_next_level(const std::vector<int>& current_levels,
                                 std::uint64_t& rng_state) const;

  LevelDescription describe_level(int level) const;
  std::vector<LevelDescription> describe_levels(const std::vector<int>& levels) const;

private:
  void build_level_lookup();
  int first_level_for_track(std::size_t track_index) const;
  std::vector<int> levels_in_scope(std::size_t track_index,
                                   int current_level,
                                   int phase_digit) const;
  static int weighted_pick(const std::vector<int>& weights, std::uint64_t& rng_state);
  const Entry* entry_for_level(int level) const;

  std::vector<Entry> index_;
  std::vector<TrackInfo> tracks_;
  std::unordered_map<int, std::size_t> level_lookup_;
};

CatalogIndex build_builtin_catalog_index();



} // namespace ear::resources
