#include "note.hpp"

#include "../src/rng.hpp"
#include "common.hpp"
#include "params.hpp"
#include "pathways.hpp"
#include "prompt_utils.hpp"
#include "../include/ear/question_bundle_v2.hpp"
#include "../include/ear/midi_clip.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>

using Beats = ear::Beats;

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

std::vector<int> base_degrees() {
  return {0, 1, 2, 3, 4, 5, 6};
}

int pick_degree(const NoteParams& params, std::uint64_t& rng_state,
                const std::optional<int>& previous) {
  auto allowed = params.degrees; 

  if (params.avoid_repeat && previous.has_value() && allowed.size() > 1) {
    allowed.erase(std::remove(allowed.begin(), allowed.end(), previous.value()), allowed.end());
    if (allowed.empty()) {
      allowed = base_degrees();
    }
  }

  int idx = rand_int(rng_state, 0, static_cast<int>(allowed.size()) - 1);
  return allowed[static_cast<std::size_t>(idx)];
}

std::vector<int> get_pathway(int d, bool incomplete = false){ 
  if (d == 0) {return {0}; }
  if (d == 1) {return {1, 0}; } 
  if (d == 2) {if (!incomplete) {return {2, 1, 0}; } else {return {2,0};} }
  if (d == 3) {if (!incomplete) {return {3, 2, 1, 0};} else {return {3,0};} }
  if (d == 4) {if (!incomplete) {return {4, 5, 6, 7}; } else {return {4,7};} }
  if (d == 5) {if (!incomplete) {return {5, 6, 7}; } else {return {5,7};} }
  if (d == 6) {return {6, 7}; } 
  return {7}; 
}

std::vector<int> get_pathway_midi (int degree, int midi, int tonic_midi, bool incomplete = false){
  int tonic_below_midi = midi - (midi - tonic_midi) % 12; 
  std::vector<int> pw = get_pathway(degree, incomplete);
  for (int& p : pw) {
    p = drills::degree_to_offset(p);
    p+= tonic_below_midi; 
  }
  return pw;
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
    tonic_midi - params.range_down, 
    tonic_midi + params.range_up
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

  //-----------------------------------------------------
  // PREPARE QUESTION AND ANSWER
  //-----------------------------------------------------
  ear::MelodyAnswerV2 note_answer = ear::MelodyAnswerV2{std::vector<int>{degree}};
  ear::MelodyQuestionV2 note_question = ear::MelodyQuestionV2{
   tonic_midi, spec_.key, spec_.quality, std::vector<int>{degree}
  };

  //-----------------------------------------------------------------
  // GENERATE MIDI-CLIP
  //-----------------------------------------------------------------
  MidiClipBuilder b(params.bpm, 480);
  auto melody_track = b.add_track("melody", 0, params.program);

  Beats beat = Beats{0}; // CURRENT BEAT
  if (!params.tonic_anchor){
    b.add_note(melody_track, beat, Beats{params.note_beats}, midi, params.velocity);
  }
  
  //-------------------------------------------------------------------
  // USE PATHWAYS: PLAYS NOTE WITH RESOLUTION TO TONIC
  //-------------------------------------------------------------------
  if (params.pathway && !params.tonic_anchor){
    std::vector<int> pathway = get_pathway_midi(degree, midi, tonic_midi, params.incomplete);
    // REMOVE ROOT: E.G. 2 -> 1,0 INSTEAD OF 2 -> 2,1,0
    if (!params.pathway_repeat_lead){ pathway.erase(pathway.begin()); }

    if (!pathway.empty()) {
      beat.advance_by(params.note_beats + params.pathway_rest); 
      for (int p : pathway) { 
        b.add_note(melody_track, beat, Beats{params.pathway_beats}, p, params.velocity);
        beat.advance_by(params.pathway_beats);
      } 
    }
  }

  //-------------------------------------------------------------------
  // TONIC ANCHOR: PLAYS NOTE IN REFERENCE TO TONIC (BEFORE OR AFTER)
  //-------------------------------------------------------------------
  if (params.use_anchor){
    // FIND ANCHOR TONIC JUST BELOW TO MIDI
    int shift = (midi - tonic_midi) % 12;
    int anchor_pitch = midi - shift; 
    if (params.tonic_anchor_include_octave){
      anchor_pitch += 12 * rand_int(rng_state, 0, 1); // ALLOW EITHER ABOVE OR BELOW
    }

    // IF VA HAS NO VALUE CHOOSE RANDOMLY
    using TA = NoteParams::TonicAnchor;
    auto ta = params.tonic_anchor.value_or(rand_int(rng_state, 0, 1) == 0 ? TA::Before : TA::After);

    // TONIC COMES BEFORE OR AFTER NOTE
    switch (ta){
      case TA::Before:
        b.add_note(melody_track, beat, Beats{params.note_beats}, anchor_pitch, params.velocity);
        beat.advance_by(params.note_beats);
        b.add_note(melody_track, beat, Beats{params.note_beats}, midi, params.velocity);
        break;

      case TA::After:
        b.add_note(melody_track, beat, Beats{params.note_beats}, midi, params.velocity);
        beat.advance_by(params.note_beats);
        b.add_note(melody_track, beat, Beats{params.note_beats}, anchor_pitch, params.velocity);
        break;
      
      default:
        break;
    }
  }

  //-----------------------------------------------------------------
  // GENERATE QUESTION BUNDLE
  //-----------------------------------------------------------------
  ear::QuestionBundle bundle;
  bundle.question_id = "place-holder";
  bundle.question_id.clear();
  bundle.correct_answer = note_answer;
  bundle.question = note_question;
  bundle.prompt_clip = b.build();

  return bundle;
}

} // namespace ear
