#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

namespace ear {

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
};

struct SessionSummary {
  std::string session_id;
  nlohmann::json totals;
  nlohmann::json by_category;
  nlohmann::json results;
};

} // namespace ear

