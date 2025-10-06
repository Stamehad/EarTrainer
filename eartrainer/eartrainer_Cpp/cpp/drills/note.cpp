#include "note.hpp"

#include "common.hpp"
#include "../src/rng.hpp"
#include "pathways.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace ear {
namespace {

const pathways::PathwayOptions* resolve_pathway(const SessionSpec& spec, int degree) {
  auto scale = pathways::infer_scale_type(spec.key);
  return pathways::find_pathway(pathways::default_bank(), scale, degree);
}

constexpr int kDefaultTempoBpm = 120;
constexpr double kDefaultStepBeats = 1.0;
constexpr double kDefaultRestBeats = 0.5;
constexpr int kRestPitch = -1;
constexpr int kRestVelocity = 0;

bool flag_param(const SessionSpec& spec, const std::string& key, bool fallback) {
  if (!spec.sampler_params.contains(key)) {
    return fallback;
  }
  const auto& node = spec.sampler_params[key];
  if (node.is_boolean()) {
    return node.get<bool>();
  }
  if (node.is_number_integer()) {
    return node.get<int>() != 0;
  }
  return fallback;
}

double beats_param(const SessionSpec& spec, const std::string& key, double fallback) {
  if (!spec.sampler_params.contains(key)) {
    return fallback;
  }
  const auto& node = spec.sampler_params[key];
  if (node.is_number_float()) {
    return node.get<double>();
  }
  if (node.is_number_integer()) {
    return static_cast<double>(node.get<int>());
  }
  return fallback;
}

int tempo_param(const SessionSpec& spec, const std::string& key) {
  if (spec.sampler_params.contains(key)) {
    const auto& node = spec.sampler_params[key];
    if (node.is_number_float()) {
      return std::max(1, static_cast<int>(std::lround(node.get<double>())));
    }
    if (node.is_number_integer()) {
      return std::max(1, node.get<int>());
    }
  }
  if (spec.tempo_bpm.has_value()) {
    return std::max(1, spec.tempo_bpm.value());
  }
  return kDefaultTempoBpm;
}

int beats_to_ms(double beats, int tempo_bpm) {
  if (beats <= 0.0) {
    return 0;
  }
  int tempo = std::max(1, tempo_bpm);
  double quarter_ms = 60000.0 / static_cast<double>(tempo);
  return static_cast<int>(std::lround(quarter_ms * beats));
}

std::vector<int> pathway_resolution_degrees(const pathways::PathwayPattern& pattern, int degree,
                                            bool include_lead) {
  std::vector<int> result;
  result.reserve(pattern.degrees.size());
  bool skipped_self = false;
  for (int path_degree : pattern.degrees) {
    if (!include_lead) {
      if (!skipped_self && drills::normalize_degree_index(path_degree) ==
                               drills::normalize_degree_index(degree)) {
        skipped_self = true;
        continue;
      }
    }
    result.push_back(path_degree);
  }
  return result;
}

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

void NoteDrill::configure(const SessionSpec& /*spec*/) {
  last_degree_.reset();
  last_midi_.reset();
}

DrillOutput NoteDrill::next_question(const SessionSpec& spec, std::uint64_t& rng_state) {
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

  int tonic_midi = drills::central_tonic_midi(spec.key);

  nlohmann::json q_payload = nlohmann::json::object();
  q_payload["degree"] = degree;
  q_payload["midi"] = midi;
  q_payload["tonic_midi"] = tonic_midi;

  nlohmann::json answer_payload = nlohmann::json::object();
  answer_payload["degree"] = degree;

  bool pathways_requested = flag_param(spec, "use_pathway", false);
  const pathways::PathwayOptions* pathway =
      pathways_requested ? resolve_pathway(spec, degree) : nullptr;
  bool pathways_active = pathways_requested && pathway != nullptr;
  bool repeat_lead = pathways_active && flag_param(spec, "pathway_repeat_lead", false);

  std::string tempo_key = pathways_active ? "pathway_tempo_bpm" : "note_tempo_bpm";
  int tempo_bpm = tempo_param(spec, tempo_key);
  double note_beats = pathways_active ? beats_param(spec, "pathway_step_beats", kDefaultStepBeats)
                                      : beats_param(spec, "note_step_beats", kDefaultStepBeats);
  int note_duration_ms = beats_to_ms(note_beats, tempo_bpm);
  if (note_duration_ms <= 0) {
    note_duration_ms = beats_to_ms(kDefaultStepBeats, tempo_bpm);
  }

  PromptPlan plan;
  plan.modality = "midi";
  plan.tempo_bpm = tempo_bpm;
  plan.count_in = false;
  plan.notes.push_back({midi, note_duration_ms, std::nullopt, std::nullopt});

  if (pathways_active) {
    double rest_beats = beats_param(spec, "pathway_rest_beats", kDefaultRestBeats);
    int rest_ms = beats_to_ms(rest_beats, tempo_bpm);
    if (rest_ms > 0) {
      // Use sentinel pitch/velocity to represent silence between the lead note and pathway.
      plan.notes.push_back({kRestPitch, rest_ms, kRestVelocity, std::nullopt});
    }

    auto resolution_degrees = pathway_resolution_degrees(pathway->primary, degree, repeat_lead);
    for (int resolution_degree : resolution_degrees) {
      int resolution_midi = drills::degree_to_midi(spec, resolution_degree);
      plan.notes.push_back({resolution_midi, note_duration_ms, std::nullopt, std::nullopt});
    }
  }

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

  return DrillOutput{TypedPayload{"note", q_payload},
                     TypedPayload{"degree", answer_payload},
                     plan,
                     hints};
}

} // namespace ear
