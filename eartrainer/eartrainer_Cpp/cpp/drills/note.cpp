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
#include <algorithm>

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


// std::vector<int> extract_allowed(const DrillSpec& spec, const std::string& key) {
//   std::vector<int> values;
//   if (spec.j_params.is_object() && spec.j_params.contains(key)) {
//     const auto& node = spec.j_params[key];
//     if (node.is_array()) {
//       for (const auto& entry : node.get_array()) {
//         values.push_back(entry.get<int>());
//       }
//     }
//   }
//   return values;
// }

std::vector<int> base_degrees() {
  return {0, 1, 2, 3, 4, 5, 6};
}

int pick_degree(const NoteParams& params, std::uint64_t& rng_state,
                const std::optional<int>& previous) {
  auto allowed = params.allowed_degrees; 

  if (params.avoid_repeat && previous.has_value() && allowed.size() > 1) {
    allowed.erase(std::remove(allowed.begin(), allowed.end(), previous.value()), allowed.end());
    if (allowed.empty()) {
      allowed = base_degrees();
    }
  }

  int idx = rand_int(rng_state, 0, static_cast<int>(allowed.size()) - 1);
  return allowed[static_cast<std::size_t>(idx)];
}

} // namespace

//====================================================================
// INITIALIZE NOTEDRILL
//====================================================================
void NoteDrill::configure(const DrillSpec& spec) {
  spec_ = spec;
  params = std::get<NoteParams>(spec_.params);
  last_degree_.reset();
  last_midi_.reset();
  tonic_midi = drills::central_tonic_midi(spec_.key);
  midi_range = {
    tonic_midi - params.range_below_semitones, 
    tonic_midi + params.range_above_semitones
  };

}
//====================================================================
// NEXT QUESTION -> QUESTION BUNDLE
//====================================================================
QuestionBundle NoteDrill::next_question(std::uint64_t& rng_state) {
  // PICK DEGREE IN 0...6 (OPTIONALLY AVOID REPETITIONS)
  int degree = pick_degree(params, rng_state, last_degree_);
  last_degree_ = degree;

  // ASSIGN MIDI VALUE IN RANGE
  auto candidates = drills::midi_candidates_for_degree(spec_.key, degree, midi_range);
  int midi = tonic_midi + drills::degree_to_offset(degree);
  if (!candidates.empty()) {
    if (params.avoid_repeat && last_midi_.has_value() && candidates.size() > 1) {
      candidates.erase(std::remove(candidates.begin(), candidates.end(), last_midi_.value()),
                       candidates.end());
      if (candidates.empty()) {
        candidates = drills::midi_candidates_for_degree(spec_.key, degree, midi_range);
      }
    }
    int choice = rand_int(rng_state, 0, static_cast<int>(candidates.size()) - 1);
    midi = candidates[static_cast<std::size_t>(choice)];
  }
  last_midi_ = midi;

  nlohmann::json q_payload = nlohmann::json::object();
  q_payload["degree"] = degree;
  q_payload["midi"] = midi;
  q_payload["tonic_midi"] = tonic_midi;

  nlohmann::json answer_payload = nlohmann::json::object();
  answer_payload["degree"] = degree;

  PromptPlan plan;
  plan.modality = "midi";
  plan.count_in = false; 

  //-------------------------------------------------------------------
  // USE PATHWAYS: PLAYS NOTE WITH RESOLUTION TO TONIC
  //-------------------------------------------------------------------
  auto pathway_to_midi = [&](int pw_degree) {
    int pw_offset = drills::degree_to_offset(pw_degree);
    int tonic_below_midi = midi - (midi - tonic_midi) % 12; 
    return tonic_below_midi + pw_offset;
  };
  
  if (params.use_pathway){
    const pathways::PathwayOptions* pathway = resolve_pathway(spec_, degree);
    
    int note_duration_ms = drills::beats_to_ms(params.pathway_step_beats, params.pathway_tempo_bpm);
    int rest_ms = drills::beats_to_ms(params.pathway_rest_beats, params.pathway_tempo_bpm);
    if (rest_ms > 0 && (degree != 0)) {
      drills::append_rest(plan, rest_ms);
    }
    plan.tempo_bpm = params.pathway_tempo_bpm;

    std::vector<int> p = (pathway->primary).degrees;                          // {3,2,1,0}, {5,6,7} etc
    std::transform(p.begin(), p.end(), p.begin(), pathway_to_midi);           // CONVERT TO MIDIS  
    if (!params.pathway_repeat_lead) {
      p = std::vector<int>(p.begin() + 1, p.end());
    }

    for (int note : p){
      drills::append_note(plan, note, note_duration_ms);
    }
    
  }
  
  //-------------------------------------------------------------------
  // TONIC ANCHOR: PLAYS NOTE IN REFERENCE TO TONIC (BEFORE OR AFTER)
  //-------------------------------------------------------------------
  if (params.use_anchor){ 

    plan.tempo_bpm = params.note_tempo_bpm;
    int note_duration_ms = drills::beats_to_ms(params.note_step_beats, params.note_tempo_bpm);
    
    // FIND ANCHOR TONIC JUST BELOW TO MIDI
    int shift = (midi - tonic_midi) % 12;
    int anchor_pitch = midi - shift; 
    if (params.tonic_anchor_include_octave){
      anchor_pitch += 12 * rand_int(rng_state, 0, 1); // ALLOW EITHER ABOVE OR BELOW
    }

    // IF VA HAS NO VALUE CHOOSE RANDOMLY
    using TA = NoteParams::TonicAnchor;
    auto ta = params.tonic_anchor.value_or(
      rand_int(rng_state, 0, 1) == 0
          ? TA::Before
          : TA::After);

    // TONIC COMES BEFORE OR AFTER NOTE
    switch (ta){
      case TA::Before:
        drills::append_note(plan, anchor_pitch, note_duration_ms);
        drills::append_note(plan, midi, note_duration_ms);

      case TA::After:
        drills::append_note(plan, midi, note_duration_ms);
        drills::append_note(plan, anchor_pitch, note_duration_ms);
        
    }
  }

  //-------------------------------------------------------------------
  // QUESTION BUNDLE
  //-------------------------------------------------------------------
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
