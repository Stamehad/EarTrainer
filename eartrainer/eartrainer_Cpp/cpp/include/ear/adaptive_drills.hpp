#pragma once

#include "../resources/catalog_manager2.hpp"
#include "../resources/level_catalog.hpp"
#include "drill_factory.hpp"
#include "types.hpp"
#include "question_bundle_v2.hpp"

#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>

#include "../nlohmann/json.hpp"

namespace ear {

class AdaptiveDrills {
public:
  //-----------------------------------------------------------------
  // AUXILIARY STRUCS
  //-----------------------------------------------------------------
  struct TrackPick {
    const resources::ManifestView::Lesson* node = nullptr;
    int track_index = -1;
    std::vector<int> weights;
    std::vector<int> normalized_levels;
  };
  struct ScoreSnapshot {
    double bout_average = 0.0;
    std::vector<std::optional<double>> drill_scores;
  };
  struct BoutOutcome {
    bool has_score = false;
    double bout_average = 0.0;
    double graduate_threshold = 0.0;
    bool level_up = false;
  };
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

  //-----------------------------------------------------------------
  // API METHODS
  //-----------------------------------------------------------------
  // INITIALIZE DRILL MANIFEST AND FACTORY
  explicit AdaptiveDrills(std::string resources_dir = "resources",
                          std::uint64_t seed = 1);
  // MAIN BOUT (LESSON) LOOP
  void set_bout(const std::vector<int>& track_levels);
  QuestionBundle next(); 
  ScoreSnapshot submit_feedback(const ResultReport& report);
  BoutOutcome current_bout_outcome() const;
  BoutReport end_bout() const;

  nlohmann::json diagnostic() const;
  
  // AUXILIARY (external callers normally don't need these)
  bool empty() const { return slots_.empty(); }
  std::size_t size() const { return slots_.size(); }
  std::size_t track_count() const { return resources::ManifestView::kTrackCount; }
  const std::vector<int>& last_used_track_levels() const { return last_track_levels_; }

private:
  //-----------------------------------------------------------------
  // Internal helpers (not part of public API)
  //-----------------------------------------------------------------
  TrackPick pick_track(const std::vector<int>& current_levels);
  double bout_average_score() const;
  const std::vector<std::optional<double>>& drill_running_scores() const { return drill_scores_; }
  struct Slot {
    std::string id;
    std::string family;
    DrillSpec spec;
    std::unique_ptr<DrillModule> module;
    std::uint64_t rng_state = 0;
  };

  struct DrillRuntime {
    double ema = 0.0;
    bool initialized = false;
    std::deque<double> recent_scores;
    std::size_t asked = 0;
  };

  std::string make_question_id();
  void initialize_bout(int level,
                       const std::vector<DrillSpec>& specs,
                       const std::vector<const ear::builtin::catalog_numbered::DrillEntry*>& entries,
                       const std::vector<bool>& mix_flags);
  DrillSpec make_spec_from_entry(const resources::ManifestView::Lesson& lesson,
                                 const ear::builtin::catalog_numbered::DrillEntry& drill,
                                 int ordinal) const;
  void update_drill_stats(std::size_t slot_index, double score);
  void update_overall_ema(double score);
  void handle_progress_controller();
  void adjust_mix_probability();
  bool plateau_reached(const DrillRuntime& stats) const;
  bool promote_current_slot();
  bool demote_current_slot();
  std::optional<std::size_t> adjacent_slot_index(std::size_t from, int direction) const;
  void set_current_slot(std::size_t index);
  bool is_main_slot(std::size_t index) const;
  const ear::builtin::catalog_numbered::DrillEntry* entry_for_slot(std::size_t index) const;
  struct DrillThresholds;
  DrillThresholds thresholds_for_slot(std::size_t slot_index) const;
  bool is_simplification_slot(std::size_t slot_index) const;
  double slot_ema(std::size_t slot_index, double fallback) const;

  std::uint64_t master_rng_;
  std::size_t question_counter_ = 0;
  int current_level_ = 0;
  DrillFactory& factory_;
  std::vector<Slot> slots_;
  std::vector<std::size_t> pick_counts_;
  std::optional<std::size_t> last_pick_;
  resources::ManifestView manifest_;
  const resources::ManifestView::Lesson* current_lesson_ = nullptr;
  ear::builtin::catalog_numbered::LessonType lesson_type_;
  Slot* current_slot_ = nullptr;
  std::size_t current_slot_index_ = 0;
  const ear::builtin::catalog_numbered::DrillEntry* current_drill_ = nullptr;

  // Mix state (Comfort Controller)
  // - mix_: whether a complementary (easier) drill is available for blending
  // - mix_slot_: pointer to the mix drill slot (if appended)
  // - mix_slot_index_: index of mix slot in slots_
  // - mix_prob_: probability to serve the mix slot (adapted towards target EMA)
  bool mix_ = false;
  Slot* mix_slot_ = nullptr;
  std::optional<std::size_t> mix_slot_index_;
  double mix_prob_ = 1.0 / 6.0;
  
  // Per-slot metadata and runtime stats
  // - slot_entries_: pointer map back to the lesson drill entries
  // - slot_is_mix_: whether a slot is the mix slot
  // - slot_runtime_: EMA, counts, recent window for PC decisions
  // - overall_ema_: global EMA for CC (comfort) control
  std::vector<const ear::builtin::catalog_numbered::DrillEntry*> slot_entries_;
  std::vector<bool> slot_is_mix_;
  std::vector<DrillRuntime> slot_runtime_;
  double overall_ema_ = 0.0;
  bool overall_ema_initialized_ = false;

  int current_track_index_ = -1;
  std::vector<int> last_track_levels_;
  std::vector<int> last_track_weights_;
  std::optional<resources::ManifestView::TrackPick> last_track_pick_;
  std::unordered_map<std::string, std::size_t> question_slot_index_;
  
  // Bout accumulation (for reporting only)
  double bout_score_sum_ = 0.0;
  std::size_t bout_score_count_ = 0;
  std::vector<std::optional<double>> drill_scores_;
  std::optional<int> active_track_index_;
  bool bout_finished_ = false;
  int question_limit_ = 0;
  int bout_questions_asked_ = 0;

  static constexpr double kScoreEmaAlpha = 0.2;
  static constexpr double kLevelUpThreshold = 0.8;
  static constexpr int kDefaultQuestionTarget = 10;
  
  // Progress Controller (PC) thresholds and EMA smoothing
  static constexpr double kDrillEmaAlpha = 0.9;     // per-drill EMA smoothing
  static constexpr int kPlateauWindow = 8;          // recent scores window
  static constexpr double kPlateauDelta = 0.01;     // min improvement considered progress
  static constexpr double kPlateauPromoteEma = 0.78;// plateau promotion threshold for simplifications
  static constexpr int kWarmupQuestionThreshold = 6;// demote consideration warmup
  static constexpr double kDefaultPromoteEma = 0.85;// default promote EMA
  static constexpr double kDefaultDemoteEma = 0.60; // default demote EMA
  static constexpr int kDefaultMinQuestions = 10;   // default min questions per drill

  // Comfort Controller (CC) parameters
  static constexpr double kOverallEmaAlpha = 0.95;  // overall EMA smoothing
  static constexpr double kTargetOverallEma = 0.80; // comfort target
  static constexpr double kMixGain = 0.2;           // proportional gain for mix probability
  static constexpr double kMixProbMin = 0.10;       // clamp min
  static constexpr double kMixProbMax = 0.60;       // clamp max
};

} // namespace ear
