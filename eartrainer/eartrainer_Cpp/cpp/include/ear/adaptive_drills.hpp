#pragma once

#include "ear/track_selector.hpp"
#include "ear/drill_factory.hpp"
#include "ear/types.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <optional>

#include <nlohmann/json.hpp>

namespace ear {

class AdaptiveDrills {
public:
  explicit AdaptiveDrills(std::string resources_dir = "resources",
                          std::uint64_t seed = 1);

  void set_bout(const std::vector<int>& track_levels);
  void set_bout_from_json(const std::vector<int>& track_levels, const nlohmann::json& document);
  struct TrackPick {
    int phase_digit = -1;
    int picked_track = -1;
    std::vector<int> weights;
  };
  TrackPick pick_track(const std::vector<int>& current_levels);
  QuestionBundle next();
  nlohmann::json diagnostic() const;

  bool empty() const { return slots_.empty(); }
  std::size_t size() const { return slots_.size(); }
  std::size_t track_count() const { return track_phase_catalogs_.size(); }
  const std::vector<int>& last_used_track_levels() const { return last_track_levels_; }

private:
  struct Slot {
    std::string id;
    std::string family;
    DrillSpec spec;
    std::unique_ptr<DrillModule> module;
    std::uint64_t rng_state = 0;
  };

  std::string make_question_id();
  void initialize_bout(int level, const std::vector<DrillSpec>& specs);

  std::filesystem::path resources_dir_;
  std::uint64_t master_rng_;
  std::size_t question_counter_ = 0;
  int current_level_ = 0;
  DrillFactory& factory_;
  std::vector<Slot> slots_;
  std::vector<std::size_t> pick_counts_;
  std::optional<std::size_t> last_pick_;
  std::vector<adaptive::TrackPhaseCatalog> track_phase_catalogs_;

  // Track-selection diagnostics/state
  std::optional<int> last_track_pick_;
  std::vector<int> last_track_weights_;
  std::optional<int> last_phase_digit_;
  std::optional<bool> phase_consistent_;
  std::vector<int> last_track_levels_;
  std::optional<std::string> track_catalog_error_;

  // helpers
  static int weighted_pick(const std::vector<int>& weights, std::uint64_t& rng_state);
  std::vector<int> levels_in_scope_for_track(int track_index, int current_level, int phase_digit) const;
  int first_level_for_track(int track_index) const;
  std::vector<int> normalize_track_levels(const std::vector<int>& track_levels) const;
};

} // namespace ear
