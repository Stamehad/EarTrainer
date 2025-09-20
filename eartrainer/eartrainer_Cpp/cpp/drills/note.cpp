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
    return false;
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

  nlohmann::json data = nlohmann::json::object();
  data["degree"] = degree;
  return AbstractSample{"note", data};
}

DrillModule::DrillOutput NoteDrill::make_question(const SessionSpec& spec,
                                                  const AbstractSample& sample) {
  int degree = sample.degrees["degree"].get<int>();
  int tonic_midi = drills::central_tonic_midi(spec.key);
  int midi = drills::degree_to_midi(spec, degree);

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
