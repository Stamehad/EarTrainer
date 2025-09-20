#include "chord.hpp"

#include "common.hpp"
#include "../src/rng.hpp"

#include <algorithm>
#include <limits>
#include <vector>

namespace ear {

namespace {

std::vector<int> extract_allowed(const SessionSpec& spec, const std::string& key) {
  std::vector<int> values;
  if (spec.sampler_params.contains(key)) {
    const auto& node = spec.sampler_params[key];
    if (node.is_array()) {
      for (const auto& entry : node.get_array()) {
        values.push_back(entry.get<int>());
      }
    }
  }
  return values;
}

std::vector<int> base_degrees() {
  return {0, 1, 2, 3, 4, 5, 6};
}

bool avoid_repetition(const SessionSpec& spec) {
  if (!spec.sampler_params.contains("chord_avoid_repeat")) {
    if (!spec.sampler_params.contains("avoid_repeat")) {
      return false;
    }
    return spec.sampler_params["avoid_repeat"].get<bool>();
  }
  return spec.sampler_params["chord_avoid_repeat"].get<bool>();
}

int pick_degree(const SessionSpec& spec, std::uint64_t& rng_state,
                const std::optional<int>& previous) {
  auto allowed = extract_allowed(spec, "chord_allowed_degrees");
  if (allowed.empty()) {
    allowed = extract_allowed(spec, "allowed_degrees");
  }
  if (allowed.empty()) {
    allowed = base_degrees();
  }

  if (avoid_repetition(spec) && previous.has_value() && allowed.size() > 1) {
    allowed.erase(std::remove(allowed.begin(), allowed.end(), previous.value()), allowed.end());
    if (allowed.empty()) {
      allowed = base_degrees();
    }
  }

  int idx = rand_int(rng_state, 0, static_cast<int>(allowed.size()) - 1);
  return allowed[static_cast<std::size_t>(idx)];
}

nlohmann::json build_degrees_payload(int root_degree, bool add_seventh = false) {
  nlohmann::json tones = nlohmann::json::array();
  tones.push_back(root_degree);
  tones.push_back(root_degree + 2);
  tones.push_back(root_degree + 4);
  if (add_seventh) {
    tones.push_back(root_degree + 6);
  }
  nlohmann::json payload = nlohmann::json::object();
  payload["root"] = root_degree;
  payload["degrees"] = tones;
  payload["quality"] = "triad";
  payload["add_seventh"] = add_seventh;
  return payload;
}

} // namespace

AbstractSample ChordSampler::next(const SessionSpec& spec, std::uint64_t& rng_state) {
  int degree = pick_degree(spec, rng_state, last_degree_);
  last_degree_ = degree;

  bool add_seventh = false;
  if (spec.sampler_params.contains("add_seventh")) {
    add_seventh = spec.sampler_params["add_seventh"].get<bool>();
  }

  auto payload = build_degrees_payload(degree, add_seventh);
  return AbstractSample{"chord", payload};
}

DrillModule::DrillOutput
ChordDrill::make_question(const SessionSpec& spec, const AbstractSample& sample) {
  const auto& degrees = sample.degrees["degrees"].get_array();
  PromptPlan plan;
  plan.modality = "midi";
  plan.tempo_bpm = spec.tempo_bpm;
  plan.count_in = false;

  nlohmann::json midi_tones = nlohmann::json::array();
  int previous = std::numeric_limits<int>::min();
  for (const auto& degree_json : degrees) {
    int degree = degree_json.get<int>();
    int midi = drills::degree_to_midi(spec, degree);
    if (!plan.notes.empty() && midi <= previous) {
      while (midi <= previous) {
        midi += 12;
      }
    }
    previous = midi;
    midi_tones.push_back(midi);
    plan.notes.push_back({midi, 500, std::nullopt, std::nullopt});
  }

  nlohmann::json question_payload = nlohmann::json::object();
  question_payload["root_degree"] = sample.degrees["root"].get<int>();
  question_payload["degrees"] = sample.degrees["degrees"];
  question_payload["voicing_midi"] = midi_tones;
  question_payload["tonic_midi"] = drills::central_tonic_midi(spec.key);

  nlohmann::json answer_payload = nlohmann::json::object();
  answer_payload["root_degree"] = sample.degrees["root"].get<int>();

  nlohmann::json hints = nlohmann::json::object();
  hints["answer_kind"] = "chord_degree";
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
                                  TypedPayload{"chord_degree", answer_payload},
                                  plan,
                                  hints};
  return output;
}

} // namespace ear
