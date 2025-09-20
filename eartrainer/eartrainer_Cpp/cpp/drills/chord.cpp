#include "chord.hpp"

#include "common.hpp"
#include "../src/rng.hpp"

namespace ear {

AbstractSample ChordSampler::next(const SessionSpec& spec, std::uint64_t& rng_state) {
  (void)spec;
  int root_degree = rand_int(rng_state, 1, 7);
  nlohmann::json tones = nlohmann::json::array();
  tones.push_back(root_degree);
  tones.push_back(root_degree + 2);
  tones.push_back(root_degree + 4);

  static const char* quality_table[7] = {"major",      "minor",      "minor",   "major",
                                         "major",      "minor",      "diminished"};
  static const char* roman_table[7] = {"I", "ii", "iii", "IV", "V", "vi", "vii°"};

  int idx = (root_degree - 1) % 7;
  nlohmann::json degrees = nlohmann::json::object();
  degrees["degrees"] = tones;
  degrees["quality"] = quality_table[idx];
  degrees["label"] = roman_table[idx];
  return AbstractSample{"chord", degrees};
}

DrillModule::DrillOutput
ChordDrill::make_question(const SessionSpec& spec, const AbstractSample& sample) {
  const auto& degrees = sample.degrees["degrees"].get_array();
  PromptPlan plan;
  plan.modality = "midi";
  plan.tempo_bpm = spec.tempo_bpm;
  plan.count_in = false;

  nlohmann::json midi_tones = nlohmann::json::array();
  for (const auto& degree_json : degrees) {
    int degree = degree_json.get<int>();
    int midi = drills::degree_to_midi(spec, degree);
    midi_tones.push_back(midi);
    plan.notes.push_back({midi, 500, std::nullopt, std::nullopt});
  }

  nlohmann::json question_payload = nlohmann::json::object();
  question_payload["tones"] = midi_tones;
  question_payload["quality"] = sample.degrees["quality"];

  nlohmann::json answer_payload = nlohmann::json::object();
  answer_payload["label"] = sample.degrees["label"];
  answer_payload["quality"] = sample.degrees["quality"];

  nlohmann::json hints = nlohmann::json::object();
  hints["answer_kind"] = "chord_label";
  nlohmann::json allowed = nlohmann::json::array();
  allowed.push_back("Replay");
  allowed.push_back("GuideTone");
  hints["allowed_assists"] = allowed;
  nlohmann::json budget = nlohmann::json::object();
  for (const auto& entry : spec.assistance_policy) {
    budget[entry.first] = entry.second;
  }
  hints["assist_budget"] = budget;

  DrillModule::DrillOutput output{TypedPayload{"chord", question_payload},
                                  TypedPayload{"chord_label", answer_payload},
                                  plan,
                                  hints};
  return output;
}

} // namespace ear
