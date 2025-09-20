#include "note.hpp"

#include "common.hpp"
#include "../src/rng.hpp"

#include <algorithm>
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
  if (!spec.sampler_params.contains("avoid_repeat")) {
    return true;
  }
  return spec.sampler_params["avoid_repeat"].get<bool>();
}

int pick_degree(const SessionSpec& spec, std::uint64_t& rng_state,
                const std::optional<int>& previous) {
  auto allowed = extract_allowed(spec, "allowed_degrees");
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

} // namespace

AbstractSample NoteSampler::next(const SessionSpec& spec, std::uint64_t& rng_state) {
  int degree = pick_degree(spec, rng_state, last_degree_);
  last_degree_ = degree;

  int span = 12;
  if (spec.sampler_params.contains("note_range_semitones")) {
    span = std::max(1, spec.sampler_params["note_range_semitones"].get<int>());
  }

  auto candidates = drills::midi_candidates_for_degree(spec, degree, span);
  int midi = drills::central_tonic_midi(spec.key) + drills::degree_to_offset(degree);
  if (!candidates.empty()) {
    if (avoid_repetition(spec) && last_midi_.has_value() && candidates.size() > 1) {
      candidates.erase(std::remove(candidates.begin(), candidates.end(), last_midi_.value()),
                       candidates.end());
      if (candidates.empty()) {
        candidates = drills::midi_candidates_for_degree(spec, degree, span);
      }
    }
    int choice = rand_int(rng_state, 0, static_cast<int>(candidates.size()) - 1);
    midi = candidates[static_cast<std::size_t>(choice)];
  }
  last_midi_ = midi;

  nlohmann::json data = nlohmann::json::object();
  data["degree"] = degree;
  data["midi"] = midi;
  return AbstractSample{"note", data};
}

DrillModule::DrillOutput NoteDrill::make_question(const SessionSpec& spec,
                                                  const AbstractSample& sample) {
  int degree = sample.degrees["degree"].get<int>();
  int tonic_midi = drills::central_tonic_midi(spec.key);
  int midi = sample.degrees.contains("midi") ? sample.degrees["midi"].get<int>()
                                              : drills::degree_to_midi(spec, degree);

  nlohmann::json q_payload = nlohmann::json::object();
  q_payload["degree"] = degree;
  q_payload["midi"] = midi;
  q_payload["tonic_midi"] = tonic_midi;

  nlohmann::json answer_payload = nlohmann::json::object();
  answer_payload["degree"] = degree;

  PromptPlan plan;
  plan.modality = "midi";
  plan.notes.push_back({midi, 500, std::nullopt, std::nullopt});
  plan.tempo_bpm = spec.tempo_bpm;
  plan.count_in = false;

  nlohmann::json hints = nlohmann::json::object();
  hints["answer_kind"] = "degree";
  nlohmann::json allowed = nlohmann::json::array();
  allowed.push_back("Replay");
  hints["allowed_assists"] = allowed;
  nlohmann::json budget = nlohmann::json::object();
  for (const auto& entry : spec.assistance_policy) {
    budget[entry.first] = entry.second;
  }
  hints["assist_budget"] = budget;

  DrillOutput output{TypedPayload{"note", q_payload},
                     TypedPayload{"degree", answer_payload},
                     plan,
                     hints};
  return output;
}

} // namespace ear
