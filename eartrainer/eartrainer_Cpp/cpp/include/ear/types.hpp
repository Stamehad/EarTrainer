#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <stdexcept>

#include "../nlohmann/json.hpp"

namespace ear {

namespace detail {

inline double clip01(double value) {
  return std::clamp(value, 0.0, 1.0);
}

} // namespace detail

enum class SessionMode {
  Manual,
  Adaptive,
  LevelInspector
};

inline std::string to_string(SessionMode mode) {
  switch (mode) {
    case SessionMode::Manual: return "manual";
    case SessionMode::Adaptive: return "adaptive";
    case SessionMode::LevelInspector: return "level_inspector";
  }
  return "manual";
}

inline SessionMode session_mode_from_string(const std::string& value) {
  if (value == "manual") {
    return SessionMode::Manual;
  }
  if (value == "adaptive") {
    return SessionMode::Adaptive;
  }
  if (value == "level_inspector") {
    return SessionMode::LevelInspector;
  }
  throw std::invalid_argument("Unknown session mode: " + value);
}

struct SessionSpec {
  std::string version;
  std::string drill_kind;
  std::string key;
  std::optional<int> tempo_bpm;
  int n_questions;
  std::string generation;
  std::unordered_map<std::string, int> assistance_policy;
  nlohmann::json sampler_params;
  std::uint64_t seed;
  SessionMode mode = SessionMode::Manual;
  bool adaptive = false;
  bool level_inspect = false;
  std::vector<int> track_levels;
  std::optional<int> inspect_level;
  std::optional<int> inspect_tier;
};

struct LevelCatalogEntry {
  int level = 0;
  int tier = 0;
  std::string label;
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
  std::optional<nlohmann::json> midi_clip;
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
    int question_count;
    std::unordered_map<std::string, int> assists_used;
    std::optional<int> first_input_rt_ms;
  } metrics;

  struct AttemptDetail {
    std::string label;
    bool correct = false;
    int attempts = 0;
    std::optional<TypedPayload> answer_fragment;
    std::optional<TypedPayload> expected_fragment;
  };

  std::vector<AttemptDetail> attempts;

  struct ScoreResult {
    double aggregate = 0.0;
    std::vector<double> per_attempt;
  };

  double score(double attempts_weight = 0.7, int fast_rt_ms = 1000, int mid_rt_ms = 5000) const;
  ScoreResult score_breakdown(double attempts_weight = 0.7, int fast_rt_ms = 1000,
                              int mid_rt_ms = 5000) const;
};

struct SessionSummary {
  std::string session_id;
  nlohmann::json totals;
  nlohmann::json by_category;
  nlohmann::json results;
};

struct MemoryPackage {
  SessionSummary summary;
  struct AdaptiveData {
    bool has_score = false;
    double bout_average = 0.0;
    double graduate_threshold = 0.0;
    bool level_up = false;
    struct DrillInfo {
      std::string family;
      std::optional<double> ema_score;
    };
    std::unordered_map<std::string, DrillInfo> drills;
    struct LevelProposal {
      int track_index = -1;
      std::string track_name;
      int current_level = 0;
      std::optional<int> suggested_level;
    };
    std::optional<LevelProposal> level;
  };
  std::optional<AdaptiveData> adaptive;
};

inline ResultReport::ScoreResult ResultReport::score_breakdown(double attempts_weight,
                                                               int fast_rt_ms,
                                                               int mid_rt_ms) const {
  ScoreResult score;
  const double weight = detail::clip01(attempts_weight);
  const auto response_score = [&](double rt_ms) -> double {
    if (mid_rt_ms <= fast_rt_ms) {
      return rt_ms <= static_cast<double>(fast_rt_ms) ? 1.0 : 0.0;
    }
    const double denominator = static_cast<double>(mid_rt_ms - fast_rt_ms);
    const double raw =
        1.0 - (rt_ms - static_cast<double>(fast_rt_ms)) / (2.0 * denominator);
    return detail::clip01(raw);
  };

  double aggregate = 0.0;
  if (correct && metrics.question_count > 0 && metrics.attempts > 0) {
    const double attempts_score =
        detail::clip01(static_cast<double>(metrics.question_count) /
                       static_cast<double>(metrics.attempts));
    const double rt_score = response_score(static_cast<double>(metrics.rt_ms));
    aggregate = weight * attempts_score + (1.0 - weight) * rt_score;
  }
  score.aggregate = aggregate;

  if (attempts.empty()) {
    score.per_attempt.push_back(aggregate);
    return score;
  }

  const std::size_t attempt_count = attempts.size();
  const double per_attempt_rt =
      attempt_count > 0 ? static_cast<double>(metrics.rt_ms) / static_cast<double>(attempt_count)
                        : static_cast<double>(metrics.rt_ms);

  for (const auto& attempt : attempts) {
    double attempt_score = 0.0;
    if (attempt.correct) {
      const double attempt_attempts =
          attempt.attempts > 0 ? static_cast<double>(attempt.attempts) : 1.0;
      const double attempt_attempts_score =
          detail::clip01(1.0 / std::max(1.0, attempt_attempts));
      const double attempt_rt_score = response_score(per_attempt_rt);
      attempt_score = weight * attempt_attempts_score + (1.0 - weight) * attempt_rt_score;
    }
    score.per_attempt.push_back(attempt_score);
  }

  return score;
}

inline double ResultReport::score(double attempts_weight, int fast_rt_ms, int mid_rt_ms) const {
  return score_breakdown(attempts_weight, fast_rt_ms, mid_rt_ms).aggregate;
}

} // namespace ear
