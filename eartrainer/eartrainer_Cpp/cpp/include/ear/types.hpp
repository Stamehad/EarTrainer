#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

namespace ear {

namespace detail {

inline double clip01(double value) {
  return std::clamp(value, 0.0, 1.0);
}

} // namespace detail

struct SessionSpec {
  std::string version;
  std::string drill_kind;
  std::string key;
  int range_min;
  int range_max;
  std::optional<int> tempo_bpm;
  int n_questions;
  std::string generation;
  std::unordered_map<std::string, int> assistance_policy;
  nlohmann::json sampler_params;
  std::uint64_t seed;
  bool adaptive = false;
  std::vector<int> track_levels;
};

struct Note {
  int pitch;
  int dur_ms;
  std::optional<int> vel;
  std::optional<bool> tie;
};

struct PromptPlan {
  std::string modality;
  std::vector<Note> notes;
  std::optional<int> tempo_bpm;
  std::optional<bool> count_in;
};

struct TypedPayload {
  std::string type;
  nlohmann::json payload;
};

struct QuestionBundle {
  std::string question_id;
  TypedPayload question;
  TypedPayload correct_answer;
  std::optional<PromptPlan> prompt;
  nlohmann::json ui_hints;
};

struct AssistBundle {
  std::string question_id;
  std::string kind;
  std::optional<PromptPlan> prompt;
  nlohmann::json ui_delta;
};

struct ResultReport {
  std::string question_id;
  TypedPayload final_answer;
  bool correct;
  struct Metrics {
    int rt_ms;
    int attempts;
    std::unordered_map<std::string, int> assists_used;
    std::optional<int> first_input_rt_ms;
  } metrics;

  double score(int question_count, double attempts_weight = 0.7, int fast_rt_ms = 1000,
               int mid_rt_ms = 5000) const;
};

struct SessionSummary {
  std::string session_id;
  nlohmann::json totals;
  nlohmann::json by_category;
  nlohmann::json results;
};

inline double ResultReport::score(int question_count, double attempts_weight, int fast_rt_ms,
                                  int mid_rt_ms) const {
  if (!correct) {
    return 0.0;
  }
  if (question_count <= 0 || metrics.attempts <= 0) {
    return 0.0;
  }

  const double weight = detail::clip01(attempts_weight);
  const double attempts_score =
      detail::clip01(static_cast<double>(question_count) / static_cast<double>(metrics.attempts));

  double rt_score = 1.0;
  if (mid_rt_ms > fast_rt_ms) {
    const double denominator = static_cast<double>(mid_rt_ms - fast_rt_ms);
    const double raw =
        1.0 - (static_cast<double>(metrics.rt_ms) - static_cast<double>(fast_rt_ms)) /
                  (2.0 * denominator);
    rt_score = detail::clip01(raw);
  } else {
    rt_score = metrics.rt_ms <= fast_rt_ms ? 1.0 : 0.0;
  }

  return weight * attempts_score + (1.0 - weight) * rt_score;
}

} // namespace ear
