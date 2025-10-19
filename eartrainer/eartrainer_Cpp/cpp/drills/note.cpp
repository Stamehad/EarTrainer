#include "note.hpp"

#include "../src/rng.hpp"
#include "common.hpp"
#include "params.hpp"
#include "pathways.hpp"
#include "prompt_utils.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace ear {
namespace {

const pathways::PathwayOptions* resolve_pathway(const DrillSpec& spec, int degree) {
  auto scale = pathways::infer_scale_type(spec.key);
  return pathways::find_pathway(pathways::default_bank(), scale, degree);
}

constexpr int kDefaultTempoBpm = 120;
constexpr double kDefaultStepBeats = 1.0;
constexpr double kDefaultRestBeats = 0.5;

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

std::vector<int> base_degrees() {
  return {0, 1, 2, 3, 4, 5, 6};
}

bool avoid_repetition(const DrillSpec& spec) {
  if (!spec.params.is_object() || !spec.params.contains("avoid_repeat")) {
    return true;
  }
  return spec.params["avoid_repeat"].get<bool>();
}

int pick_degree(const DrillSpec& spec, std::uint64_t& rng_state,
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

void NoteDrill::configure(const DrillSpec& spec) {
  spec_ = spec;
  last_degree_.reset();
  last_midi_.reset();
}

QuestionBundle NoteDrill::next_question(std::uint64_t& rng_state) {
  int degree = pick_degree(spec_, rng_state, last_degree_);
  last_degree_ = degree;

  auto candidates = drills::midi_candidates_for_degree(spec_, degree, 12);
  int midi = drills::central_tonic_midi(spec_.key) + drills::degree_to_offset(degree);
  if (!candidates.empty()) {
    if (avoid_repetition(spec_) && last_midi_.has_value() && candidates.size() > 1) {
      candidates.erase(std::remove(candidates.begin(), candidates.end(), last_midi_.value()),
                       candidates.end());
      if (candidates.empty()) {
        candidates = drills::midi_candidates_for_degree(spec_, degree, 12);
      }
    }
    int choice = rand_int(rng_state, 0, static_cast<int>(candidates.size()) - 1);
    midi = candidates[static_cast<std::size_t>(choice)];
  }
  last_midi_ = midi;

  int tonic_midi = drills::central_tonic_midi(spec_.key);

  nlohmann::json q_payload = nlohmann::json::object();
  q_payload["degree"] = degree;
  q_payload["midi"] = midi;
  q_payload["tonic_midi"] = tonic_midi;

  nlohmann::json answer_payload = nlohmann::json::object();
  answer_payload["degree"] = degree;

  bool pathways_requested = drills::param_flag(spec_, "use_pathway", false);
  const pathways::PathwayOptions* pathway =
      pathways_requested ? resolve_pathway(spec_, degree) : nullptr;
  bool pathways_active = pathways_requested && pathway != nullptr;
  bool repeat_lead =
      pathways_active && drills::param_flag(spec_, "pathway_repeat_lead", false);

  std::string tempo_key = pathways_active ? "pathway_tempo_bpm" : "note_tempo_bpm";
  int tempo_bpm = drills::param_tempo(spec_, tempo_key, kDefaultTempoBpm);
  double note_beats =
      pathways_active ? drills::param_double(spec_, "pathway_step_beats", kDefaultStepBeats)
                      : drills::param_double(spec_, "note_step_beats", kDefaultStepBeats);
  int note_duration_ms = drills::beats_to_ms(note_beats, tempo_bpm);
  if (note_duration_ms <= 0) {
    note_duration_ms = drills::beats_to_ms(kDefaultStepBeats, tempo_bpm);
  }

  PromptPlan plan;
  plan.modality = "midi";
  plan.tempo_bpm = tempo_bpm;
  plan.count_in = false;

  std::string tonic_anchor = "none";
  if (spec_.params.contains("tonic_anchor") && spec_.params["tonic_anchor"].is_string()) {
    tonic_anchor = spec_.params["tonic_anchor"].get<std::string>();
    std::transform(tonic_anchor.begin(), tonic_anchor.end(), tonic_anchor.begin(), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
    });
  }

  bool anchor_before = false;
  bool anchor_after = false;
  bool anchor_include_octave = false;

  if (!pathways_active) {
    if (tonic_anchor == "before") {
      anchor_before = true;
    } else if (tonic_anchor == "after") {
      anchor_after = true;
    } else if (tonic_anchor == "both") {
      anchor_before = rand_int(rng_state, 0, 1) == 0;
      anchor_after = !anchor_before;
    }
    anchor_include_octave = drills::param_flag(spec_, "tonic_anchor_include_octave", false);
  }

  auto choose_anchor_pitch = [&](int base_pitch) -> int {
    int tonic_pitch = base_pitch;
    if (anchor_include_octave && rand_int(rng_state, 0, 1) == 1) {
      int octave_pitch = drills::clamp_to_range(tonic_pitch + 12, spec_.range_min, spec_.range_max);
      if (octave_pitch > tonic_pitch) {
        tonic_pitch = octave_pitch;
      }
    }
    return tonic_pitch;
  };

  if (anchor_before) {
    drills::append_note(plan, choose_anchor_pitch(tonic_midi), note_duration_ms);
  }

  drills::append_note(plan, midi, note_duration_ms);

  if (anchor_after) {
    drills::append_note(plan, choose_anchor_pitch(tonic_midi), note_duration_ms);
  }

  if (pathways_active) {
    double rest_beats = drills::param_double(spec_, "pathway_rest_beats", kDefaultRestBeats);
    int rest_ms = drills::beats_to_ms(rest_beats, tempo_bpm);
    if (rest_ms > 0) {
      drills::append_rest(plan, rest_ms);
    }

    auto pick_resolution_pitch = [&](int resolution_degree, int reference_pitch) {
      int span = std::max(12, spec_.range_max - spec_.range_min);
      auto candidates = drills::midi_candidates_for_degree(spec_, resolution_degree, span);
      if (!candidates.empty()) {
        auto it = std::min_element(
            candidates.begin(), candidates.end(),
            [&](int lhs, int rhs) {
              int lhs_diff = std::abs(lhs - reference_pitch);
              int rhs_diff = std::abs(rhs - reference_pitch);
              if (lhs_diff == rhs_diff) {
                return lhs < rhs;
              }
              return lhs_diff < rhs_diff;
            });
        return *it;
      }
      return drills::degree_to_midi(spec_, resolution_degree);
    };

    auto resolution_degrees = pathway_resolution_degrees(pathway->primary, degree, repeat_lead);
    int reference_pitch = midi;
    for (int resolution_degree : resolution_degrees) {
      int resolution_midi = pick_resolution_pitch(resolution_degree, reference_pitch);
      reference_pitch = resolution_midi;
      drills::append_note(plan, resolution_midi, note_duration_ms);
    }
  }

  nlohmann::json hints = nlohmann::json::object();
  hints["answer_kind"] = "degree";
  nlohmann::json allowed = nlohmann::json::array();
  allowed.push_back("Replay");
  hints["allowed_assists"] = allowed;
  nlohmann::json budget = nlohmann::json::object();
  for (const auto& entry : spec_.assistance_policy) {
    budget[entry.first] = entry.second;
  }
  hints["assist_budget"] = budget;

  QuestionBundle bundle;
  bundle.question_id.clear();
  bundle.question = TypedPayload{"note", q_payload};
  bundle.correct_answer = TypedPayload{"degree", answer_payload};
  bundle.prompt = plan;
  bundle.ui_hints = hints;
  return bundle;
}

} // namespace ear
