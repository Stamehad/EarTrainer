#include "ear/adaptive_drills.hpp"

#include "ear/drill_factory.hpp"
#include "resources/catalog_manager2.hpp"
#include "resources/level_catalog.hpp"
#include "rng.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <mutex>
#include <sstream>
#include <string_view>
#include <utility>
#include <cstdlib>
#include <iostream>

namespace ear {

namespace {

bool adaptive_debug_enabled() {
  static bool enabled = [] {
    const char* env = std::getenv("EAR_DEBUG_ADAPTIVE");
    if (!env) {
      env = std::getenv("EAR_DEBUG_SESSION");
    }
    if (!env) {
      return false;
    }
    std::string value(env);
    return !(value.empty() || value == "0" || value == "false" || value == "FALSE");
  }();
  return enabled;
}

void adaptive_debug(const std::string& message) {
  if (adaptive_debug_enabled()) {
    std::cerr << "[adaptive] " << message << std::endl;
  }
}

void ensure_factory_registered() {
  static std::once_flag flag;
  std::call_once(flag, []() {
    auto& factory = DrillFactory::instance();
    register_builtin_drills(factory);
  });
}

int track_index_for_family(std::string_view name) {
  if (name == "melody") {
    return 0;
  }
  if (name == "harmony") {
    return 1;
  }
  if (name == "chord") {
    return 2;
  }
  return -1;
}

bool uses_sequential_slots(ear::builtin::catalog_numbered::LessonType type) {
  using Type = ear::builtin::catalog_numbered::LessonType;
  return type == Type::Lesson || type == Type::Warmup;
}

} // namespace

AdaptiveDrills::AdaptiveDrills(std::string resources_dir, std::uint64_t seed)
    : master_rng_(seed == 0 ? 1 : seed),
      factory_(DrillFactory::instance()),
      manifest_(resources::manifest()),
      last_track_levels_(resources::ManifestView::kTrackCount, 0),
      last_track_weights_(resources::ManifestView::kTrackCount, 0) {
  ensure_factory_registered();
}

AdaptiveDrills::TrackPick AdaptiveDrills::pick_track(const std::vector<int>& current_levels) {
  const auto pick_info = manifest_.pick_track(current_levels, master_rng_);

  TrackPick pick;
  pick.node = pick_info.node;
  pick.track_index = pick_info.track_index;
  pick.weights = pick_info.weights;
  pick.normalized_levels = pick_info.normalized_levels;

  last_track_pick_ = pick_info.node ? std::optional{pick_info} : std::nullopt;
  last_track_levels_ = pick.normalized_levels;
  last_track_weights_ = pick.weights;

  return pick;
}

void AdaptiveDrills::set_bout(const std::vector<int>& track_levels) {
  auto pick = pick_track(track_levels);
  if (!pick.node) {
    throw std::runtime_error("AdaptiveDrills: unable to select track/bout");
  }

  current_lesson_ = pick.node;
  lesson_type_ = current_lesson_->type; 
  current_track_index_ = pick.track_index;
  question_limit_ = 0;
  bout_questions_asked_ = 0;
  bout_finished_ = false;

  rebuild_current_lesson_slots();
}

void AdaptiveDrills::rebuild_current_lesson_slots() {
  if (!current_lesson_) {
    throw std::runtime_error("AdaptiveDrills: no lesson selected");
  }
  mix_ = current_lesson_->meta.mix >= 0;
  mix_slot_index_.reset();
  mix_slot_ = nullptr;

  std::vector<DrillSpec> specs;
  std::vector<const ear::builtin::catalog_numbered::DrillEntry*> slot_entries;
  std::vector<bool> slot_mix_flags;
  specs.reserve(current_lesson_->drills.size() + 1);
  slot_entries.reserve(current_lesson_->drills.size() + 1);
  slot_mix_flags.reserve(current_lesson_->drills.size() + 1);
  int ordinal = 0;
  for (const auto& drill : current_lesson_->drills) {
    if (!drill.build) continue;
    specs.push_back(make_spec_from_entry(*current_lesson_, drill, ordinal));
    slot_entries.push_back(&drill);
    slot_mix_flags.push_back(false);
    ++ordinal;
  }
  std::optional<std::size_t> pending_mix_index;

  if (mix_) {
    const auto* mix_lesson =
        manifest_.entry(current_lesson_->meta.mix, current_lesson_->family());
    if (mix_lesson && !mix_lesson->drills.empty()) {
      const auto& drill = mix_lesson->drills.back();
      if (drill.build) {
        specs.push_back(make_spec_from_entry(*mix_lesson, drill, ordinal));
        slot_entries.push_back(&drill);
        slot_mix_flags.push_back(true);
        pending_mix_index = specs.size() - 1;
      } else {
        mix_ = false;
      }
    } else {
      mix_ = false;
    }
  }

  initialize_bout(current_lesson_->lesson, specs, slot_entries, slot_mix_flags);
  active_track_index_ = current_track_index_;

  set_current_slot(0);

  if (mix_ && pending_mix_index.has_value() && *pending_mix_index < slots_.size()) {
    mix_slot_index_ = pending_mix_index;
    mix_slot_ = &slots_[*pending_mix_index];
  } else {
    mix_slot_index_.reset();
    mix_slot_ = nullptr;
    mix_ = false;
  }
}

bool AdaptiveDrills::set_lesson(int lesson_number) {
  const char* family = ear::builtin::catalog_numbered::lesson_to_family(lesson_number);
  if (std::string_view(family) == "none") {
    return false;
  }
  const auto* lesson = manifest_.entry(lesson_number, family);
  if (!lesson) {
    return false;
  }
  current_lesson_ = lesson;
  lesson_type_ = current_lesson_->type;
  current_track_index_ = track_index_for_family(family);
  question_limit_ = kDefaultQuestionTarget;
  bout_questions_asked_ = 0;
  bout_finished_ = false;
  mix_slot_index_.reset();
  mix_slot_ = nullptr;
  last_track_pick_.reset();

  if (last_track_levels_.size() != resources::ManifestView::kTrackCount) {
    last_track_levels_.assign(resources::ManifestView::kTrackCount, 0);
  }
  if (current_track_index_ >= 0 &&
      static_cast<std::size_t>(current_track_index_) < resources::ManifestView::kTrackCount) {
    last_track_levels_.assign(resources::ManifestView::kTrackCount, 0);
    last_track_levels_[static_cast<std::size_t>(current_track_index_)] = lesson_number;
  }
  last_track_weights_.assign(resources::ManifestView::kTrackCount, 0);

  rebuild_current_lesson_slots();
  adaptive_debug("set_lesson lesson=" + std::to_string(lesson_number));
  return true;
}

void AdaptiveDrills::initialize_bout(
    int level,
    const std::vector<DrillSpec>& specs,
    const std::vector<const ear::builtin::catalog_numbered::DrillEntry*>& entries,
    const std::vector<bool>& mix_flags) {
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
  slot_entries_ = entries;
  slot_is_mix_ = mix_flags;
  if (slot_entries_.size() < slots_.size()) slot_entries_.resize(slots_.size(), nullptr);
  if (slot_is_mix_.size() < slots_.size()) slot_is_mix_.resize(slots_.size(), false);
  slot_runtime_.assign(slots_.size(), DrillRuntime{});
  slot_completed_.assign(slots_.size(), false);
  eligible_min_questions_ = 0;
  update_question_limit();
  current_slot_ = nullptr;
  overall_ema_ = 0.0;
  overall_ema_initialized_ = false;
}

DrillSpec AdaptiveDrills::make_spec_from_entry(
    const resources::ManifestView::Lesson& lesson,
    const ear::builtin::catalog_numbered::DrillEntry& drill,
    int ordinal) const {
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

QuestionBundle AdaptiveDrills::next() {
  if (slots_.empty()) {
    throw std::runtime_error("AdaptiveDrills::next called before set_bout or with empty bout");
  }
  if (bout_finished_) {
    throw std::runtime_error("AdaptiveDrills::next called after bout finished");
  }
  const bool sequential = uses_sequential_slots(lesson_type_);

  if (sequential){
    if (!current_slot_) {
      throw std::runtime_error("AdaptiveDrills: no active slot for lesson");
    }
    // LESSON: USE CURRENT SLOT
    Slot* slot = current_slot_;
    std::size_t slot_index = current_slot_index_;

    // IF MIX: SAMPLE CURRENT AND MIX
    if (mix_ && mix_slot_ && mix_slot_index_.has_value()) {
      const double roll = rand_unit(master_rng_);
      if (roll < mix_prob_) {
        slot = mix_slot_;
        slot_index = *mix_slot_index_;
      }
    }
    if (slot_index >= pick_counts_.size()) {
      throw std::runtime_error("AdaptiveDrills: slot index out of range");
    }
    pick_counts_[slot_index] += 1;
    last_pick_ = slot_index;

    // GET QUESTION BUNDLE
    auto bundle = slot->module->next_question(slot->rng_state);
    std::string question_id = make_question_id();
    question_slot_index_[question_id] = slot_index;
    adaptive_debug("lesson "
                   + std::to_string(current_lesson_ ? current_lesson_->lesson : -1)
                   + " slot=" + std::to_string(slot_index)
                   + " question=" + question_id);
    bundle.question_id = std::move(question_id);
    return bundle;

  } else {
    // SAMPLE UNIFORMLY
    const int max_index = static_cast<int>(slots_.size()) - 1;
    int pick = rand_int(master_rng_, 0, max_index);
    auto& slot = slots_[static_cast<std::size_t>(pick)];
    pick_counts_[static_cast<std::size_t>(pick)] += 1;
    last_pick_ = static_cast<std::size_t>(pick);

    // GET QUESTION BUNDLE
    auto bundle = slot.module->next_question(slot.rng_state);
    std::string question_id = make_question_id();
    question_slot_index_[question_id] = static_cast<std::size_t>(pick);
    bundle.question_id = std::move(question_id);
    return bundle;
  } 
}

std::string AdaptiveDrills::make_question_id() {
  std::ostringstream oss;
  oss << "ad-";
  oss.width(3);
  oss.fill('0');
  oss << ++question_counter_;
  return oss.str();
}

void AdaptiveDrills::set_current_slot(std::size_t index) {
  if (index >= slots_.size()) {
    throw std::out_of_range("AdaptiveDrills: slot index out of range");
  }
  current_slot_index_ = index;
  current_slot_ = &slots_[index];
  current_drill_ = entry_for_slot(index);
  adaptive_debug("set_current_slot index=" + std::to_string(index) + " id=" + current_slot_->id);
}

const ear::builtin::catalog_numbered::DrillEntry*
AdaptiveDrills::entry_for_slot(std::size_t index) const {
  if (index >= slot_entries_.size()) return nullptr;
  return slot_entries_[index];
}

bool AdaptiveDrills::is_main_slot(std::size_t index) const {
  if (index >= slot_is_mix_.size()) return true;
  if (slot_is_mix_[index]) {
    return false;
  }
  if (index < slot_completed_.size() && slot_completed_[index]) {
    return false;
  }
  return true;
}

void AdaptiveDrills::update_question_limit() {
  eligible_min_questions_ = 0;
  const int fallback = kDefaultMinQuestions;
  for (std::size_t i = 0; i < slot_entries_.size(); ++i) {
    if (i < slot_is_mix_.size() && slot_is_mix_[i]) {
      continue;
    }
    const auto* entry = slot_entries_[i];
    if (!entry) {
      continue;
    }
    int min_q = entry->q > 0 ? entry->q : fallback;
    eligible_min_questions_ += min_q;
  }
  if (eligible_min_questions_ <= 0) {
    eligible_min_questions_ = fallback * static_cast<int>(slots_.size());
    if (eligible_min_questions_ <= 0) {
      eligible_min_questions_ = fallback;
    }
  }

  const double elig_cutoff =
      std::ceil(kProgressFactor * static_cast<double>(eligible_min_questions_));
  double mix_factor = 1.0;
  if (mix_ && mix_prob_ < 1.0) {
    const double denom = std::max(0.05, 1.0 - mix_prob_);
    mix_factor = 1.0 / denom;
  }
  const int total_cutoff =
      static_cast<int>(std::ceil(elig_cutoff * mix_factor));
  question_limit_ = std::max(eligible_min_questions_, total_cutoff);
  if (question_limit_ <= 0) {
    question_limit_ = eligible_min_questions_;
  }
  adaptive_debug("question_limit updated elig_min=" + std::to_string(eligible_min_questions_) +
                 " mix_prob=" + std::to_string(mix_prob_) +
                 " limit=" + std::to_string(question_limit_));
}

std::optional<std::size_t> AdaptiveDrills::adjacent_slot_index(std::size_t from,
                                                               int direction) const {
  if (slots_.empty()) return std::nullopt;
  if (direction == 0) return from;
  std::size_t idx = from;
  if (direction > 0) {
    while (++idx < slots_.size()) {
      if (is_main_slot(idx)) return idx;
    }
  } else {
    while (idx > 0) {
      --idx;
      if (is_main_slot(idx)) return idx;
    }
  }
  return std::nullopt;
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

  ++bout_questions_asked_;
  if (question_limit_ > 0 && bout_questions_asked_ >= question_limit_) {
    bout_finished_ = true;
  }

  update_drill_stats(slot_index, score);
  update_overall_ema(score);
  if (slot_index == current_slot_index_) {
    handle_progress_controller();
  }
  adjust_mix_probability();

  ScoreSnapshot snapshot;
  snapshot.bout_average = bout_average_score();
  snapshot.drill_scores = drill_scores_;
  return snapshot;
}

double AdaptiveDrills::bout_average_score() const {
  if (bout_score_count_ == 0) {
    return 0.0;
  }
  return bout_score_sum_ / static_cast<double>(bout_score_count_);
}

void AdaptiveDrills::update_drill_stats(std::size_t slot_index, double score) {
  if (slot_index >= slot_runtime_.size()) return;
  auto& stats = slot_runtime_[slot_index];
  stats.asked += 1;
  if (!stats.initialized) {
    stats.ema = score;
    stats.initialized = true;
  } else {
    stats.ema = kDrillEmaAlpha * stats.ema + (1.0 - kDrillEmaAlpha) * score;
  }
  stats.recent_scores.push_back(score);
  if (stats.recent_scores.size() > static_cast<std::size_t>(kPlateauWindow)) {
    stats.recent_scores.pop_front();
  }
}

void AdaptiveDrills::update_overall_ema(double score) {
  if (!overall_ema_initialized_) {
    overall_ema_ = score;
    overall_ema_initialized_ = true;
  } else {
    overall_ema_ = kOverallEmaAlpha * overall_ema_ + (1.0 - kOverallEmaAlpha) * score;
  }
}

struct AdaptiveDrills::DrillThresholds {
  int min_questions = kDefaultMinQuestions;
  double promote_ema = kDefaultPromoteEma;
  double demote_ema = kDefaultDemoteEma;
};

AdaptiveDrills::DrillThresholds
AdaptiveDrills::thresholds_for_slot(std::size_t slot_index) const {
  DrillThresholds thr;
  const auto* entry = entry_for_slot(slot_index);
  if (entry && entry->q > 0) {
    thr.min_questions = entry->q;
  }
  return thr;
}

bool AdaptiveDrills::plateau_reached(const DrillRuntime& stats) const {
  if (stats.recent_scores.size() < static_cast<std::size_t>(kPlateauWindow)) return false;
  auto [min_it, max_it] =
      std::minmax_element(stats.recent_scores.begin(), stats.recent_scores.end());
  return (*max_it - *min_it) < kPlateauDelta;
}

bool AdaptiveDrills::promote_current_slot() {
  if (current_slot_index_ < slot_completed_.size() && is_main_slot(current_slot_index_)) {
    slot_completed_[current_slot_index_] = true;
    adaptive_debug("slot completed index=" + std::to_string(current_slot_index_));
  }
  auto next = adjacent_slot_index(current_slot_index_, +1);
  if (!next) {
    bout_finished_ = true;
    adaptive_debug("bout finished after promotions");
    return false;
  }
  adaptive_debug("advancing to slot index=" + std::to_string(*next));
  set_current_slot(*next);
  return true;
}

bool AdaptiveDrills::demote_current_slot() {
  auto prev = adjacent_slot_index(current_slot_index_, -1);
  if (!prev) return false;
  adaptive_debug("demoting to slot index=" + std::to_string(*prev));
  set_current_slot(*prev);
  return true;
}

bool AdaptiveDrills::is_simplification_slot(std::size_t) const {
  return lesson_type_ != ear::builtin::catalog_numbered::LessonType::Lesson;
}

double AdaptiveDrills::slot_ema(std::size_t slot_index, double fallback) const {
  if (slot_index < slot_runtime_.size() && slot_runtime_[slot_index].initialized) {
    return slot_runtime_[slot_index].ema;
  }
  return fallback;
}

void AdaptiveDrills::handle_progress_controller() {
  if (current_slot_index_ >= slot_runtime_.size()) return;
  auto& stats = slot_runtime_[current_slot_index_];
  const auto thresholds = thresholds_for_slot(current_slot_index_);

  const bool has_met_min =
      stats.asked >= static_cast<std::size_t>(std::max(1, thresholds.min_questions));
  const bool can_promote = has_met_min && stats.ema >= thresholds.promote_ema;

  const bool plateau_ok =
      has_met_min && is_simplification_slot(current_slot_index_) &&
      stats.ema >= kPlateauPromoteEma && plateau_reached(stats);

  const bool should_demote =
      stats.asked >= static_cast<std::size_t>(kWarmupQuestionThreshold) &&
      stats.ema <= thresholds.demote_ema;

  if (can_promote || plateau_ok) {
    promote_current_slot();
  } else if (should_demote) {
    demote_current_slot();
  }
}

void AdaptiveDrills::adjust_mix_probability() {
  if (!mix_ || !mix_slot_index_.has_value()) {
    mix_prob_ = std::clamp(mix_prob_, kMixProbMin, kMixProbMax);
    update_question_limit();
    return;
  }
  if (!overall_ema_initialized_) return;
  const double mix_ema = slot_ema(*mix_slot_index_, 0.90);
  const double new_ema = slot_ema(current_slot_index_, mix_ema);
  const double err = kTargetOverallEma - overall_ema_;
  const double gap = std::max(0.05, std::fabs(mix_ema - new_ema));
  mix_prob_ = std::clamp(mix_prob_ + kMixGain * err / gap, kMixProbMin, kMixProbMax);
  update_question_limit();
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
  if (active_track_index_.has_value() &&
      *active_track_index_ >= 0 &&
      *active_track_index_ < resources::ManifestView::kTrackCount) {
    LevelRecommendation recommendation;
    recommendation.track_index = *active_track_index_;
    recommendation.track_name =
        resources::ManifestView::kTrackNames[static_cast<std::size_t>(*active_track_index_)];
    recommendation.current_level = current_level_;
    recommendation.suggested_level = std::nullopt;
    report.level = std::move(recommendation);
  }
  return report;
}

nlohmann::json AdaptiveDrills::diagnostic() const {
  nlohmann::json info = nlohmann::json::object();
  info["level"] = current_level_;
  info["slots"] = static_cast<int>(slots_.size());
  info["questions_emitted"] = static_cast<int>(question_counter_);
  info["question_limit"] = question_limit_;
  info["eligible_min_questions"] = eligible_min_questions_;
  info["questions_answered"] = bout_questions_asked_;
  info["bout_finished"] = bout_finished_;
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
    info["level_suggested"] = report.level->suggested_level.has_value()
                                  ? nlohmann::json(*report.level->suggested_level)
                                  : nlohmann::json(nullptr);
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
    info["last_track_pick_index"] = last_track_pick_->track_index;
    info["last_track_pick_level"] =
        last_track_pick_->node ? last_track_pick_->node->lesson : 0;
  } else {
    info["last_track_pick_index"] = nullptr;
    info["last_track_pick_level"] = nullptr;
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

  info["tracks"] = nlohmann::json::array();
  return info;
}

} // namespace ear
