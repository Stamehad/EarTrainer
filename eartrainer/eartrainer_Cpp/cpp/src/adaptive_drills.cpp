#include "ear/adaptive_drills.hpp"

#include "ear/drill_factory.hpp"
#include "resources/catalog_manager.hpp"
#include "rng.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <mutex>
#include <sstream>
#include <utility>

namespace ear {

namespace {

void ensure_factory_registered() {
  static std::once_flag flag;
  std::call_once(flag, []() {
    auto& factory = DrillFactory::instance();
    register_builtin_drills(factory);
  });
}

} // namespace

AdaptiveDrills::AdaptiveDrills(std::string resources_dir, std::uint64_t seed)
    : resources_dir_(std::move(resources_dir)),
      master_rng_(seed == 0 ? 1 : seed),
      factory_(DrillFactory::instance()) {
  ensure_factory_registered();

  if (!resources_dir_.is_absolute()) {
    resources_dir_ = std::filesystem::current_path() / resources_dir_;
  }

  catalog_index_ = resources::build_builtin_catalog_index();
  using_builtin_catalogs_ = true;
  track_catalog_error_.reset();
}

AdaptiveDrills::TrackPick AdaptiveDrills::pick_track(const std::vector<int>& current_levels) {
  AdaptiveDrills::TrackPick pick;
  phase_consistent_.reset();

  auto pick_info = catalog_index_.pick_next_level(current_levels, master_rng_);

  last_track_levels_ = pick_info.normalized_levels;
  last_track_weights_ = pick_info.weights;
  pick.weights = pick_info.weights;
  pick.normalized_levels = pick_info.normalized_levels;
  pick.phase_digit = pick_info.phase_digit;
  pick.picked_track = pick_info.track_index;
  pick.next_level = pick_info.level;

  if (pick.phase_digit >= 0) {
    last_phase_digit_ = pick.phase_digit;
  } else {
    last_phase_digit_.reset();
  }
  if (pick.picked_track >= 0) {
    last_track_pick_ = pick.picked_track;
  } else {
    last_track_pick_.reset();
  }

  std::optional<int> observed_phase;
  bool consistent = true;
  for (int level : last_track_levels_) {
    if (level <= 0) continue;
    int phase = (std::abs(level) / 10) % 10;
    if (!observed_phase.has_value()) {
      observed_phase = phase;
    } else if (*observed_phase != phase) {
      consistent = false;
      if (phase < *observed_phase) {
        observed_phase = phase;
      }
    }
  }
  phase_consistent_ = consistent;

  return pick;
}

void AdaptiveDrills::set_bout(const std::vector<int>& track_levels) {
  if (catalog_index_.empty()) {
    if (track_catalog_error_.has_value()) {
      throw std::runtime_error("AdaptiveDrills: track catalogs unavailable (" + *track_catalog_error_ + ")");
    }
    throw std::runtime_error("AdaptiveDrills: no track catalogs available");
  }

  auto pick = pick_track(track_levels);
  if (pick.phase_digit < 0) {
    throw std::runtime_error("AdaptiveDrills: unable to determine active phase from levels");
  }
  if (pick.picked_track < 0) {
    throw std::runtime_error("AdaptiveDrills: no eligible track could be selected");
  }

  int track_index = pick.picked_track;
  int target_level = pick.next_level;

  const auto& specs = catalog_index_.drills_for_level(target_level);
  initialize_bout(target_level, specs);
  active_track_index_ = track_index;
}

void AdaptiveDrills::initialize_bout(int level, const std::vector<DrillSpec>& specs) {
  slots_.clear();
  question_counter_ = 0;
  current_level_ = level;
  pick_counts_.clear();
  last_pick_.reset();
  question_slot_index_.clear();
  bout_score_sum_ = 0.0;
  bout_score_count_ = 0;
  drill_scores_.clear();
  active_track_index_.reset();

  if (specs.empty()) {
    throw std::runtime_error("Adaptive bout has no drills configured");
  }

  std::uint64_t seed = master_rng_;
  for (const auto& spec : specs) {
    auto assignment = factory_.create(spec);
    Slot slot;
    slot.id = std::move(assignment.id);
    slot.family = std::move(assignment.family);
    slot.spec = assignment.spec;
    slot.module = std::move(assignment.module);
    slot.rng_state = advance_rng(seed);
    slots_.push_back(std::move(slot));
    pick_counts_.push_back(0);
  }

  master_rng_ = seed;
  if (slots_.empty()) {
    throw std::runtime_error("Adaptive bout initialization produced zero drills");
  }
  drill_scores_.assign(slots_.size(), std::nullopt);
}

QuestionBundle AdaptiveDrills::next() {
  if (slots_.empty()) {
    throw std::runtime_error("AdaptiveDrills::next called before set_bout or with empty bout");
  }

  const int max_index = static_cast<int>(slots_.size()) - 1;
  int pick = rand_int(master_rng_, 0, max_index);
  auto& slot = slots_[static_cast<std::size_t>(pick)];
  pick_counts_[static_cast<std::size_t>(pick)] += 1;
  last_pick_ = static_cast<std::size_t>(pick);
  auto bundle = slot.module->next_question(slot.rng_state);
  // apply_prompt_rendering(slot.spec, bundle);

  std::string question_id = make_question_id();
  question_slot_index_[question_id] = static_cast<std::size_t>(pick);
  bundle.question_id = std::move(question_id);
  return bundle;
}

std::string AdaptiveDrills::make_question_id() {
  std::ostringstream oss;
  oss << "ad-";
  oss.width(3);
  oss.fill('0');
  oss << ++question_counter_;
  return oss.str();
}

AdaptiveDrills::ScoreSnapshot AdaptiveDrills::submit_feedback(const ResultReport& report) {
  auto it = question_slot_index_.find(report.question_id);
  if (it == question_slot_index_.end()) {
    throw std::invalid_argument("AdaptiveDrills::submit_feedback unknown question_id: " + report.question_id);
  }
  const std::size_t slot_index = it->second;
  question_slot_index_.erase(it);

  const double score = report.score();
  bout_score_sum_ += score;
  bout_score_count_ += 1;

  if (slot_index >= drill_scores_.size()) {
    throw std::out_of_range("AdaptiveDrills::submit_feedback slot index out of range");
  }

  auto& current_score = drill_scores_[slot_index];
  if (current_score.has_value()) {
    current_score = kScoreEmaAlpha * score + (1.0 - kScoreEmaAlpha) * current_score.value();
  } else {
    current_score = score;
  }

  ScoreSnapshot snapshot;
  snapshot.bout_average = bout_average_score();
  snapshot.drill_scores = drill_scores_;
  return snapshot;
}

std::optional<int> AdaptiveDrills::next_level_for_track(int track_index) const {
  if (track_index < 0 || static_cast<std::size_t>(track_index) >= catalog_index_.track_count()) {
    return std::nullopt;
  }
  const auto& track = catalog_index_.tracks()[static_cast<std::size_t>(track_index)];
  const std::vector<int>* levels = nullptr;
  if (last_phase_digit_.has_value()) {
    auto phase_it = track.phases.find(*last_phase_digit_);
    if (phase_it != track.phases.end()) {
      levels = &phase_it->second;
    }
  }
  if (!levels) {
    for (const auto& [phase_key, phase_levels] : track.phases) {
      if (std::find(phase_levels.begin(), phase_levels.end(), current_level_) != phase_levels.end()) {
        levels = &phase_levels;
        break;
      }
    }
  }
  if (!levels) {
    return std::nullopt;
  }
  auto it = std::upper_bound(levels->begin(), levels->end(), current_level_);
  if (it != levels->end()) {
    return *it;
  }
  return std::nullopt;
}

double AdaptiveDrills::bout_average_score() const {
  if (bout_score_count_ == 0) {
    return 0.0;
  }
  return bout_score_sum_ / static_cast<double>(bout_score_count_);
}

AdaptiveDrills::BoutOutcome AdaptiveDrills::current_bout_outcome() const {
  const auto report = end_bout();
  BoutOutcome outcome;
  outcome.graduate_threshold = report.graduate_threshold;
  if (report.has_score) {
    outcome.has_score = true;
    outcome.bout_average = report.bout_average;
    outcome.level_up = report.level_up;
  }
  return outcome;
}

AdaptiveDrills::BoutReport AdaptiveDrills::end_bout() const {
  BoutReport report;
  report.graduate_threshold = kLevelUpThreshold;
  if (bout_score_count_ > 0) {
    report.has_score = true;
    report.bout_average = bout_average_score();
    report.level_up = report.bout_average >= kLevelUpThreshold;
  }
  report.drill_scores.reserve(slots_.size());
  for (std::size_t i = 0; i < slots_.size(); ++i) {
    DrillReport entry;
    entry.id = slots_[i].id;
    entry.family = slots_[i].family;
    if (i < drill_scores_.size()) {
      entry.score = drill_scores_[i];
    }
    report.drill_scores.push_back(std::move(entry));
  }
  if (active_track_index_.has_value()) {
    LevelRecommendation recommendation;
    recommendation.track_index = *active_track_index_;
    if (*active_track_index_ >= 0 &&
        static_cast<std::size_t>(*active_track_index_) < catalog_index_.track_count()) {
      recommendation.track_name =
          catalog_index_.track_name(static_cast<std::size_t>(*active_track_index_));
    }
    recommendation.current_level = current_level_;
    recommendation.suggested_level = next_level_for_track(*active_track_index_);
    report.level = std::move(recommendation);
  }
  return report;
}

nlohmann::json AdaptiveDrills::diagnostic() const {
  nlohmann::json info = nlohmann::json::object();
  info["resources_dir"] = resources_dir_.string();
  info["catalog_mode"] = using_builtin_catalogs_ ? "builtin" : "filesystem";
  info["level"] = current_level_;
  info["slots"] = static_cast<int>(slots_.size());
  info["questions_emitted"] = static_cast<int>(question_counter_);
  const auto report = end_bout();
  info["bout_score_average"] = report.bout_average;
  info["level_up_threshold"] = report.graduate_threshold;
  if (report.has_score) {
    info["level_up_ready"] = report.level_up;
  } else {
    info["level_up_ready"] = nullptr;
  }
  if (report.level.has_value()) {
    info["level_track_index"] = report.level->track_index;
    info["level_track_name"] = report.level->track_name;
    info["level_current"] = report.level->current_level;
    if (report.level->suggested_level.has_value()) {
      info["level_suggested"] = *report.level->suggested_level;
    } else {
      info["level_suggested"] = nullptr;
    }
  }
  nlohmann::json drill_scores = nlohmann::json::array();
  for (const auto& entry : report.drill_scores) {
    if (entry.score.has_value()) {
      drill_scores.push_back(entry.score.value());
    } else {
      drill_scores.push_back(nullptr);
    }
  }
  info["drill_scores"] = std::move(drill_scores);
  if (track_catalog_error_.has_value()) info["track_catalog_error"] = *track_catalog_error_;
  if (last_phase_digit_.has_value()) info["phase_digit"] = *last_phase_digit_; else info["phase_digit"] = nullptr;
  if (phase_consistent_.has_value()) info["phase_consistent"] = *phase_consistent_; else info["phase_consistent"] = nullptr;
  nlohmann::json weights_json = nlohmann::json::array();
  for (int w : last_track_weights_) {
    weights_json.push_back(w);
  }
  info["track_weights"] = weights_json;
  nlohmann::json level_json = nlohmann::json::array();
  for (int level : last_track_levels_) {
    level_json.push_back(level);
  }
  info["track_levels"] = level_json;
  if (last_track_pick_.has_value()) {
    info["last_track_pick"] = *last_track_pick_;
    if (*last_track_pick_ >= 0 &&
        static_cast<std::size_t>(*last_track_pick_) < catalog_index_.track_count()) {
      info["current_track"] = catalog_index_.track_name(static_cast<std::size_t>(*last_track_pick_));
    } else {
      info["current_track"] = nullptr;
    }
  } else {
    info["last_track_pick"] = nullptr;
    info["current_track"] = nullptr;
  }
  if (last_pick_.has_value()) {
    info["last_pick"] = static_cast<int>(*last_pick_);
  } else {
    info["last_pick"] = nullptr;
  }
  nlohmann::json ids = nlohmann::json::array();
  nlohmann::json families = nlohmann::json::array();
  nlohmann::json counts = nlohmann::json::array();
  for (std::size_t i = 0; i < slots_.size(); ++i) {
    ids.push_back(slots_[i].id);
    families.push_back(slots_[i].family);
    counts.push_back(static_cast<int>(i < pick_counts_.size() ? pick_counts_[i] : 0));
  }
  info["ids"] = ids;
  info["families"] = families;
  info["pick_counts"] = counts;

  // Add drills-in-scope per track for the last known phase and current level hints.
  if (last_phase_digit_.has_value()) {
    nlohmann::json tracks = nlohmann::json::array();
    const auto& track_infos = catalog_index_.tracks();
    for (std::size_t i = 0; i < track_infos.size(); ++i) {
      nlohmann::json t = nlohmann::json::object();
      t["name"] = track_infos[i].name;
      int current_level_hint = 0;
      if (i < last_track_levels_.size()) {
        current_level_hint = last_track_levels_[i];
      } else {
        current_level_hint = current_level_;
      }
      std::vector<int> scope;
      const auto phase_it = track_infos[i].phases.find(*last_phase_digit_);
      if (phase_it != track_infos[i].phases.end()) {
        const auto& phase_levels = phase_it->second;
        const int level_phase = (std::abs(current_level_hint) / 10) % 10;
        if (level_phase < *last_phase_digit_) {
          scope = phase_levels;
        } else if (level_phase == *last_phase_digit_) {
          const auto lb = std::lower_bound(phase_levels.begin(), phase_levels.end(), current_level_hint);
          scope.assign(lb, phase_levels.end());
        }
      }
      nlohmann::json lvls = nlohmann::json::array();
      for (int v : scope) lvls.push_back(v);
      t["levels_in_scope"] = lvls;
      t["current_level"] = current_level_hint;
      int weight = (i < last_track_weights_.size()) ? last_track_weights_[i] : 0;
      t["weight"] = weight;
      tracks.push_back(t);
    }
    info["tracks"] = tracks;
  }
  return info;
}

} // namespace ear
