#include "../include/ear/adaptive_drills.hpp"

#include "../include/ear/adaptive_catalog.hpp"
#include "../include/ear/track_selector.hpp"
#include "../include/ear/drill_factory.hpp"
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

  std::vector<adaptive::TrackCatalogDescriptor> descriptors;
  descriptors = adaptive::track_catalogs_from_resources(resources_dir_);

  try {
    track_phase_catalogs_ = adaptive::load_track_phase_catalogs(descriptors);
  } catch (const std::exception& ex) {
    track_phase_catalogs_.clear();
    track_catalog_error_ = ex.what();
  }
}

int AdaptiveDrills::weighted_pick(const std::vector<int>& weights, std::uint64_t& rng_state) {
  long long total = 0;
  for (int w : weights) total += (w > 0 ? w : 0);
  if (total <= 0) return -1;
  long long r = rand_int(rng_state, 1, static_cast<int>(total));
  long long acc = 0;
  for (std::size_t i = 0; i < weights.size(); ++i) {
    if (weights[i] <= 0) continue;
    acc += weights[i];
    if (r <= acc) return static_cast<int>(i);
  }
  return -1;
}

std::vector<int> AdaptiveDrills::levels_in_scope_for_track(int track_index, int current_level, int phase_digit) const {
  std::vector<int> out;
  if (track_index < 0 || static_cast<std::size_t>(track_index) >= track_phase_catalogs_.size()) return out;
  const auto& summary = track_phase_catalogs_[static_cast<std::size_t>(track_index)];
  auto it = summary.phases.find(phase_digit);
  if (it == summary.phases.end()) return out;
  const auto& phase_levels = it->second;

  const int level_phase = (current_level >= 0) ? ((current_level / 10) % 10) : ((-current_level / 10) % 10);
  if (level_phase < phase_digit) {
    // Behind the phase: all phase levels are pending
    out = phase_levels;
  } else if (level_phase == phase_digit) {
    // In the phase: levels from current and above
    auto lb = std::lower_bound(phase_levels.begin(), phase_levels.end(), current_level);
    out.assign(lb, phase_levels.end());
  } else {
    // Ahead of the phase: none
  }
  return out;
}

int AdaptiveDrills::first_level_for_track(int track_index) const {
  if (track_index < 0 || static_cast<std::size_t>(track_index) >= track_phase_catalogs_.size()) {
    return 0;
  }
  const auto& summary = track_phase_catalogs_[static_cast<std::size_t>(track_index)];
  for (const auto& [phase, levels] : summary.phases) {
    if (!levels.empty()) {
      return levels.front();
    }
  }
  return 0;
}

std::vector<int> AdaptiveDrills::normalize_track_levels(const std::vector<int>& track_levels) const {
  std::vector<int> normalized = track_levels;
  const std::size_t tracks = track_phase_catalogs_.size();
  if (tracks == 0) {
    return normalized;
  }
  if (normalized.size() < tracks) {
    normalized.resize(tracks, 0);
  } else if (normalized.size() > tracks) {
    normalized.resize(tracks);
  }
  for (std::size_t i = 0; i < tracks; ++i) {
    if (normalized[i] <= 0) {
      int fallback = first_level_for_track(static_cast<int>(i));
      if (fallback > 0) {
        normalized[i] = fallback;
      }
    }
  }
  return normalized;
}

AdaptiveDrills::TrackPick AdaptiveDrills::pick_track(const std::vector<int>& current_levels) {
  AdaptiveDrills::TrackPick pick;
  if (current_levels.size() != track_phase_catalogs_.size()) {
    throw std::invalid_argument("pick_track: current_levels size must match number of tracks");
  }

  phase_consistent_.reset();
  last_track_levels_ = current_levels;

  std::optional<int> observed_phase;
  bool consistent = true;
  for (int level : current_levels) {
    if (level <= 0) continue;
    int phase = (std::abs(level) / 10) % 10;
    if (!observed_phase.has_value()) {
      observed_phase = phase;
    } else if (*observed_phase != phase) {
      consistent = false;
      if (phase < *observed_phase) {
        observed_phase = phase; // keep smallest phase as reference
      }
    }
  }
  phase_consistent_ = consistent;

  auto sel = adaptive::compute_track_phase_weights(current_levels, track_phase_catalogs_);
  pick.phase_digit = sel.phase_digit;
  pick.weights = std::move(sel.weights);
  int idx = weighted_pick(pick.weights, master_rng_);
  pick.picked_track = idx;

  last_phase_digit_ = pick.phase_digit;
  last_track_weights_ = pick.weights;
  last_track_pick_ = (idx >= 0 ? std::optional<int>(idx) : std::nullopt);
  return pick;
}

void AdaptiveDrills::set_bout(const std::vector<int>& track_levels) {
  if (track_phase_catalogs_.empty()) {
    if (track_catalog_error_.has_value()) {
      throw std::runtime_error("AdaptiveDrills: track catalogs unavailable (" + *track_catalog_error_ + ")");
    }
    throw std::runtime_error("AdaptiveDrills: no track catalogs available");
  }

  auto normalized_levels = normalize_track_levels(track_levels);
  auto pick = pick_track(normalized_levels);
  if (pick.phase_digit < 0) {
    throw std::runtime_error("AdaptiveDrills: unable to determine active phase from levels");
  }
  if (pick.picked_track < 0) {
    throw std::runtime_error("AdaptiveDrills: no eligible track could be selected");
  }

  int track_index = pick.picked_track;
  int current_level = normalized_levels[static_cast<std::size_t>(track_index)];
  auto scope = levels_in_scope_for_track(track_index, current_level, pick.phase_digit);
  if (scope.empty()) {
    throw std::runtime_error("AdaptiveDrills: selected track has no pending levels in current phase");
  }
  int target_level = scope.front();

  const auto& descriptor = track_phase_catalogs_[static_cast<std::size_t>(track_index)];
  auto specs = adaptive::load_level_catalog(descriptor.resolved_path.string(), target_level);
  initialize_bout(target_level, specs);
  active_track_index_ = track_index;
}

void AdaptiveDrills::set_bout_from_json(const std::vector<int>& track_levels, const nlohmann::json& document) {
  auto normalized_levels = normalize_track_levels(track_levels);
  if (track_phase_catalogs_.empty()) {
    if (normalized_levels.empty()) {
      throw std::runtime_error("AdaptiveDrills: track levels required when catalogs unavailable");
    }
    last_track_levels_ = normalized_levels;
    last_track_weights_.assign(normalized_levels.size(), 0);
    last_track_pick_.reset();
    last_phase_digit_.reset();
    phase_consistent_.reset();

    int target_level = normalized_levels.front();
    if (target_level <= 0) {
      throw std::runtime_error("AdaptiveDrills: invalid target level");
    }

    auto specs = DrillSpec::load_json(document);
    auto filtered = DrillSpec::filter_by_level(specs, target_level);
    if (filtered.empty()) {
      throw std::runtime_error("No adaptive drills configured for level " + std::to_string(target_level));
    }
    active_track_index_.reset();
    initialize_bout(target_level, filtered);
    return;
  }
  auto pick = pick_track(normalized_levels);
  if (pick.phase_digit < 0 || pick.picked_track < 0) {
    throw std::runtime_error("AdaptiveDrills: cannot evaluate track selection for JSON bout");
  }

  int track_index = pick.picked_track;
  int current_level = normalized_levels[static_cast<std::size_t>(track_index)];
  auto scope = levels_in_scope_for_track(track_index, current_level, pick.phase_digit);
  if (scope.empty()) {
    throw std::runtime_error("AdaptiveDrills: selected track has no pending levels in current phase");
  }
  int target_level = scope.front();

  const auto& descriptor = track_phase_catalogs_[static_cast<std::size_t>(track_index)];

  auto specs = DrillSpec::load_json(document);
  auto filtered = DrillSpec::filter_by_level(specs, target_level);
  if (filtered.empty()) {
    throw std::runtime_error("No adaptive drills configured for level " + std::to_string(target_level));
  }
  initialize_bout(target_level, filtered);
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
  apply_prompt_rendering(slot.spec, bundle);

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
  if (track_index < 0 || static_cast<std::size_t>(track_index) >= track_phase_catalogs_.size()) {
    return std::nullopt;
  }
  const auto& catalog = track_phase_catalogs_[static_cast<std::size_t>(track_index)];
  const std::vector<int>* levels = nullptr;
  if (last_phase_digit_.has_value()) {
    auto phase_it = catalog.phases.find(*last_phase_digit_);
    if (phase_it != catalog.phases.end()) {
      levels = &phase_it->second;
    }
  }
  if (!levels) {
    for (const auto& [phase_key, phase_levels] : catalog.phases) {
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
        static_cast<std::size_t>(*active_track_index_) < track_phase_catalogs_.size()) {
      recommendation.track_name =
          track_phase_catalogs_[static_cast<std::size_t>(*active_track_index_)].descriptor.name;
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
        static_cast<std::size_t>(*last_track_pick_) < track_phase_catalogs_.size()) {
      info["current_track"] = track_phase_catalogs_[static_cast<std::size_t>(*last_track_pick_)].descriptor.name;
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
    for (std::size_t i = 0; i < track_phase_catalogs_.size(); ++i) {
      nlohmann::json t = nlohmann::json::object();
      t["name"] = track_phase_catalogs_[i].descriptor.name;
      int current_level_hint = 0;
      if (i < last_track_levels_.size()) {
        current_level_hint = last_track_levels_[i];
      } else {
        current_level_hint = current_level_;
      }
      auto scope = levels_in_scope_for_track(static_cast<int>(i), current_level_hint, *last_phase_digit_);
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
