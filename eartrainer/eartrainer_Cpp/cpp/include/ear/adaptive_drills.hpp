#pragma once

#include "track_selector.hpp"
#include "drill_factory.hpp"
#include "types.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>

#include "../nlohmann/json.hpp"

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
  struct ScoreSnapshot {
    double bout_average = 0.0;
    std::vector<std::optional<double>> drill_scores;
  };
  ScoreSnapshot submit_feedback(const ResultReport& report);
  double bout_average_score() const;
  const std::vector<std::optional<double>>& drill_running_scores() const { return drill_scores_; }
  struct BoutOutcome {
    bool has_score = false;
    double bout_average = 0.0;
    double graduate_threshold = 0.0;
    bool level_up = false;
  };
  BoutOutcome current_bout_outcome() const;
  struct DrillReport {
    std::string id;
    std::string family;
    std::optional<double> score;
  };
  struct LevelRecommendation {
    int track_index = -1;
    std::string track_name;
    int current_level = 0;
    std::optional<int> suggested_level;
  };
  struct BoutReport {
    bool has_score = false;
    double bout_average = 0.0;
    double graduate_threshold = 0.0;
    bool level_up = false;
    std::vector<DrillReport> drill_scores;
    std::optional<LevelRecommendation> level;
  };
  BoutReport end_bout() const;

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
  std::unordered_map<std::string, std::size_t> question_slot_index_;
  double bout_score_sum_ = 0.0;
  std::size_t bout_score_count_ = 0;
  std::vector<std::optional<double>> drill_scores_;
  std::optional<int> active_track_index_;

  // helpers
  static int weighted_pick(const std::vector<int>& weights, std::uint64_t& rng_state);
  std::vector<int> levels_in_scope_for_track(int track_index, int current_level, int phase_digit) const;
  int first_level_for_track(int track_index) const;
  std::vector<int> normalize_track_levels(const std::vector<int>& track_levels) const;
  std::optional<int> next_level_for_track(int track_index) const;

  static constexpr double kScoreEmaAlpha = 0.2;
  static constexpr double kLevelUpThreshold = 0.8;
};

} // namespace ear
