#include "interval.hpp"

#include "common.hpp"
#include "ear/types.hpp"
#include "ear/session_engine.hpp"
#include "../src/rng.hpp"

#include <cmath>
#include <map>

namespace ear {
namespace {

std::string interval_name(int semitone_difference) {
  static const std::map<int, std::string> names = {
      {0, "P1"},  {1, "m2"}, {2, "M2"}, {3, "m3"}, {4, "M3"}, {5, "P4"},
      {6, "TT"},  {7, "P5"}, {8, "m6"}, {9, "M6"}, {10, "m7"}, {11, "M7"},
  };
  int value = semitone_difference % 12;
  if (value < 0) {
    value += 12;
  }
  auto it = names.find(value);
  return it != names.end() ? it->second : "UNK";
}

} // namespace

AbstractSample IntervalSampler::next(const SessionSpec& spec, std::uint64_t& rng_state) {
  (void)spec;
  int root_degree = rand_int(rng_state, 1, 7);
  static const int offsets[] = {-5, -4, -3, -2, -1, 1, 2, 3, 4, 5};
  int offset = offsets[rand_int(rng_state, 0, static_cast<int>(sizeof(offsets) / sizeof(int)) - 1)];
  nlohmann::json degrees = nlohmann::json::object();
  degrees["root"] = root_degree;
  degrees["target"] = root_degree + offset;
  degrees["direction"] = offset > 0 ? "up" : "down";
  return AbstractSample{"interval", degrees};
}

DrillModule::DrillOutput IntervalDrill::make_question(const SessionSpec& spec,
                                                     const AbstractSample& sample) {
  int root_degree = sample.degrees["root"].get<int>();
  int target_degree = sample.degrees["target"].get<int>();

  int root_midi = drills::degree_to_midi(spec, root_degree);
  int target_midi = drills::degree_to_midi(spec, target_degree);

  nlohmann::json question_inner = nlohmann::json::object();
  question_inner["root_midi"] = root_midi;
  question_inner["target_midi"] = target_midi;

  int diff = target_midi - root_midi;
  nlohmann::json answer_payload = nlohmann::json::object();
  answer_payload["type"] = "interval_class";
  nlohmann::json answer_inner = nlohmann::json::object();
  answer_inner["name"] = interval_name(std::abs(diff));
  answer_payload["payload"] = answer_inner;

  PromptPlan plan;
  plan.modality = "midi";
  plan.notes.push_back({root_midi, 400, std::nullopt, std::nullopt});
  plan.notes.push_back({target_midi, 400, std::nullopt, std::nullopt});
  plan.tempo_bpm = spec.tempo_bpm;
  plan.count_in = true;

  nlohmann::json hints = nlohmann::json::object();
  hints["answer_kind"] = "interval_class";
  nlohmann::json allowed = nlohmann::json::array();
  allowed.push_back("Replay");
  allowed.push_back("GuideTone");
  hints["allowed_assists"] = allowed;
  nlohmann::json budget = nlohmann::json::object();
  for (const auto& entry : spec.assistance_policy) {
    budget[entry.first] = entry.second;
  }
  hints["assist_budget"] = budget;

  DrillModule::DrillOutput output{TypedPayload{"interval", question_inner},
                                  TypedPayload{"interval_class", answer_inner},
                                  plan,
                                  hints};
  return output;
}

} // namespace ear
