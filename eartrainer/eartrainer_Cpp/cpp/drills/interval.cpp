#include "interval.hpp"

#include "common.hpp"
#include "../src/rng.hpp"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <vector>

namespace ear {
namespace {

std::vector<int> extract_allowed(const DrillSpec& spec, const std::string& key) {
  std::vector<int> values;
  if (spec.params.is_object() && spec.params.contains(key)) {
    const auto& node = spec.params[key];
    if (node.is_array()) {
      for (const auto& entry : node.get_array()) {
        values.push_back(entry.get<int>());
      }
    }
  }
  return values;
}

std::vector<int> base_bottom_degrees() {
  return {0, 1, 2, 3, 4, 5, 6};
}

bool avoid_repetition(const DrillSpec& spec) {
  if (!spec.params.is_object() || !spec.params.contains("interval_avoid_repeat")) {
    if (!spec.params.is_object() || !spec.params.contains("avoid_repeat")) {
      return true;
    }
    return spec.params["avoid_repeat"].get<bool>();
  }
  return spec.params["interval_avoid_repeat"].get<bool>();
}

int pick_bottom_degree(const DrillSpec& spec, std::uint64_t& rng_state,
                       const std::optional<int>& previous_degree) {
  auto allowed = extract_allowed(spec, "interval_allowed_bottom_degrees");
  if (allowed.empty()) {
    allowed = extract_allowed(spec, "interval_allowed_degrees");
  }
  if (allowed.empty()) {
    allowed = extract_allowed(spec, "allowed_degrees");
  }
  if (allowed.empty()) {
    allowed = base_bottom_degrees();
  }

  if (avoid_repetition(spec) && previous_degree.has_value() && allowed.size() > 1) {
    allowed.erase(std::remove(allowed.begin(), allowed.end(), previous_degree.value()), allowed.end());
    if (allowed.empty()) {
      allowed = base_bottom_degrees();
    }
  }

  int idx = rand_int(rng_state, 0, static_cast<int>(allowed.size()) - 1);
  return allowed[static_cast<std::size_t>(idx)];
}

int pick_interval_size(const DrillSpec& spec, std::uint64_t& rng_state) {
  auto allowed = extract_allowed(spec, "interval_allowed_sizes");
  if (allowed.empty()) {
    allowed = {1, 2, 3, 4, 5, 6, 7};
  }
  int idx = rand_int(rng_state, 0, static_cast<int>(allowed.size()) - 1);
  return allowed[static_cast<std::size_t>(idx)];
}

std::string interval_name(int semitones) {
  static const std::map<int, std::string> names = {
      {0, "P1"},  {1, "m2"}, {2, "M2"}, {3, "m3"}, {4, "M3"}, {5, "P4"},
      {6, "TT"},  {7, "P5"}, {8, "m6"}, {9, "M6"}, {10, "m7"}, {11, "M7"},
      {12, "P8"},
  };
  int value = semitones % 12;
  if (value < 0) {
    value += 12;
  }
  auto it = names.find(value);
  return it != names.end() ? it->second : "UNK";
}

} // namespace

void IntervalDrill::configure(const DrillSpec& spec) {
  spec_ = spec;
  last_bottom_degree_.reset();
  last_bottom_midi_.reset();
}

QuestionBundle IntervalDrill::next_question(std::uint64_t& rng_state) {
  int bottom_degree = pick_bottom_degree(spec_, rng_state, last_bottom_degree_);
  int size = pick_interval_size(spec_, rng_state);
  int top_degree = bottom_degree + size;

  int span = 12;
  if (spec_.params.contains("interval_range_semitones")) {
    span = std::max(1, spec_.params["interval_range_semitones"].get<int>());
  }

  auto candidates = drills::midi_candidates_for_degree(spec_, bottom_degree, span);
  int bottom_midi = drills::degree_to_midi(spec_, bottom_degree);
  if (!candidates.empty()) {
    if (avoid_repetition(spec_) && last_bottom_midi_.has_value() && candidates.size() > 1) {
      candidates.erase(std::remove(candidates.begin(), candidates.end(), last_bottom_midi_.value()),
                       candidates.end());
      if (candidates.empty()) {
        candidates = drills::midi_candidates_for_degree(spec_, bottom_degree, span);
      }
    }
    int idx = rand_int(rng_state, 0, static_cast<int>(candidates.size()) - 1);
    bottom_midi = candidates[static_cast<std::size_t>(idx)];
  }

  int semitone_diff = drills::degree_to_offset(top_degree) - drills::degree_to_offset(bottom_degree);
  int top_midi = bottom_midi + semitone_diff;

  bool ascending = rand_int(rng_state, 0, 1) == 1;
  std::string orientation = ascending ? "ascending" : "descending";

  last_bottom_degree_ = bottom_degree;
  last_bottom_midi_ = bottom_midi;

  nlohmann::json question_payload = nlohmann::json::object();
  question_payload["bottom_midi"] = bottom_midi;
  question_payload["top_midi"] = top_midi;
  question_payload["bottom_degree"] = bottom_degree;
  question_payload["top_degree"] = top_degree;
  question_payload["orientation"] = orientation;

  nlohmann::json answer_payload = nlohmann::json::object();
  answer_payload["name"] = interval_name(std::abs(semitone_diff));
  answer_payload["semitones"] = std::abs(semitone_diff);
  answer_payload["size"] = size;

  PromptPlan plan;
  plan.modality = "midi";
  plan.tempo_bpm = spec_.tempo_bpm;
  plan.count_in = false;
  // Ensure the prompt sequence reflects conceptual orientation so a simple player
  // produces the intended direction without additional UI logic.
  if (orientation == "descending") {
    plan.notes.push_back({top_midi, 600, std::nullopt, std::nullopt});
    plan.notes.push_back({bottom_midi, 600, std::nullopt, std::nullopt});
  } else {
    plan.notes.push_back({bottom_midi, 600, std::nullopt, std::nullopt});
    plan.notes.push_back({top_midi, 600, std::nullopt, std::nullopt});
  }

  nlohmann::json hints = nlohmann::json::object();
  hints["answer_kind"] = "interval_class";
  nlohmann::json allowed = nlohmann::json::array();
  allowed.push_back("Replay");
  allowed.push_back("GuideTone");
  hints["allowed_assists"] = allowed;
  nlohmann::json budget = nlohmann::json::object();
  for (const auto& entry : spec_.assistance_policy) {
    budget[entry.first] = entry.second;
  }
  hints["assist_budget"] = budget;

  QuestionBundle bundle;
  bundle.question_id.clear();
  bundle.question = TypedPayload{"interval", question_payload};
  bundle.correct_answer = TypedPayload{"interval_class", answer_payload};
  bundle.prompt = plan;
  bundle.ui_hints = hints;
  return bundle;
}

} // namespace ear
