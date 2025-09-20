#include "melody.hpp"

#include <algorithm>

#include "common.hpp"
#include "../src/rng.hpp"

namespace ear {

AbstractSample MelodySampler::next(const SessionSpec& spec, std::uint64_t& rng_state) {
  int length = 3;
  if (spec.sampler_params.contains("melody_length")) {
    length = spec.sampler_params["melody_length"].get<int>();
  }
  length = std::max(2, length);

  int start_degree = rand_int(rng_state, 1, 7);
  nlohmann::json sequence = nlohmann::json::array();
  int current = start_degree;
  sequence.push_back(current);
  for (int i = 1; i < length; ++i) {
    int step = rand_int(rng_state, -2, 2);
    if (step == 0) {
      step = 1;
    }
    current += step;
    sequence.push_back(current);
  }
  nlohmann::json degrees = nlohmann::json::object();
  degrees["sequence"] = sequence;
  return AbstractSample{"melody", degrees};
}

DrillModule::DrillOutput
MelodyDrill::make_question(const SessionSpec& spec, const AbstractSample& sample) {
  const auto& seq = sample.degrees["sequence"].get_array();
  nlohmann::json midi_notes = nlohmann::json::array();
  PromptPlan plan;
  plan.modality = "midi";
  plan.tempo_bpm = spec.tempo_bpm;
  plan.count_in = true;

  for (const auto& degree_json : seq) {
    int degree = degree_json.get<int>();
    int midi = drills::degree_to_midi(spec, degree);
    midi_notes.push_back(midi);
    plan.notes.push_back({midi, 350, std::nullopt, std::nullopt});
  }

  nlohmann::json question_payload = nlohmann::json::object();
  question_payload["notes"] = midi_notes;

  nlohmann::json answer_payload = nlohmann::json::object();
  answer_payload["degrees"] = sample.degrees["sequence"];

  nlohmann::json hints = nlohmann::json::object();
  hints["answer_kind"] = "melody_notes";
  nlohmann::json allowed = nlohmann::json::array();
  allowed.push_back("Replay");
  hints["allowed_assists"] = allowed;
  nlohmann::json budget = nlohmann::json::object();
  for (const auto& entry : spec.assistance_policy) {
    budget[entry.first] = entry.second;
  }
  hints["assist_budget"] = budget;

  DrillModule::DrillOutput output{TypedPayload{"melody", question_payload},
                                  TypedPayload{"melody_notes", answer_payload},
                                  plan,
                                  hints};
  return output;
}

} // namespace ear
