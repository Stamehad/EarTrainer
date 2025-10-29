#include "interval.hpp"

#include "common.hpp"
#include "../src/rng.hpp"
#include "../include/ear/question_bundle_v2.hpp"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <vector>

namespace ear {
namespace {

std::vector<int> extract_allowed(const DrillSpec& spec, const std::string& key) {
  std::vector<int> values;
  if (spec.j_params.is_object() && spec.j_params.contains(key)) {
    const auto& node = spec.j_params[key];
    if (node.is_array()) {
      for (const auto& entry : node.get_array()) {
        values.push_back(entry.get<int>());
      }
    }
  }
  return values;
}

std::vector<int> base_bottom_degrees() {
  return {0, 1, 2, 3, 4, 5, 6};
}

bool avoid_repetition(const DrillSpec& spec) {
  if (!spec.j_params.is_object() || !spec.j_params.contains("interval_avoid_repeat")) {
    if (!spec.j_params.is_object() || !spec.j_params.contains("avoid_repeat")) {
      return true;
    }
    return spec.j_params["avoid_repeat"].get<bool>();
  }
  return spec.j_params["interval_avoid_repeat"].get<bool>();
}

int pick_bottom_degree(const DrillSpec& spec, std::uint64_t& rng_state,
                       const std::optional<int>& previous_degree) {
  auto allowed = extract_allowed(spec, "interval_allowed_bottom_degrees");
  if (allowed.empty()) {
    allowed = extract_allowed(spec, "interval_allowed_degrees");
  }
  if (allowed.empty()) {
    allowed = extract_allowed(spec, "allowed_degrees");
  }
  if (allowed.empty()) {
    allowed = base_bottom_degrees();
  }

  if (avoid_repetition(spec) && previous_degree.has_value() && allowed.size() > 1) {
    allowed.erase(std::remove(allowed.begin(), allowed.end(), previous_degree.value()), allowed.end());
    if (allowed.empty()) {
      allowed = base_bottom_degrees();
    }
  }

  int idx = rand_int(rng_state, 0, static_cast<int>(allowed.size()) - 1);
  return allowed[static_cast<std::size_t>(idx)];
}

int pick_interval_size(const DrillSpec& spec, std::uint64_t& rng_state) {
  auto allowed = extract_allowed(spec, "interval_allowed_sizes");
  if (allowed.empty()) {
    allowed = {1, 2, 3, 4, 5, 6, 7};
  }
  int idx = rand_int(rng_state, 0, static_cast<int>(allowed.size()) - 1);
  return allowed[static_cast<std::size_t>(idx)];
}

std::string get_interval_name(int semitones) {
  static const std::map<int, std::string> names = {
      {0, "P1"},  {1, "m2"}, {2, "M2"}, {3, "m3"}, {4, "M3"}, {5, "P4"},
      {6, "TT"},  {7, "P5"}, {8, "m6"}, {9, "M6"}, {10, "m7"}, {11, "M7"},
      {12, "P8"},
  };
  int value = semitones % 12;
  if (value < 0) {
    value += 12;
  }
  auto it = names.find(value);
  return it != names.end() ? it->second : "UNK";
}

} // namespace

void IntervalDrill::configure(const DrillSpec& spec) {
  spec_ = spec;
  params = std::get<IntervalParams>(spec.params);
  last_bottom_degree_.reset();
  last_bottom_midi_.reset();
}

QuestionBundle IntervalDrill::next_question(std::uint64_t& rng_state) {
  int bottom_degree = pick_bottom_degree(spec_, rng_state, last_bottom_degree_);
  int size = pick_interval_size(spec_, rng_state);
  int tonic_midi = drills::central_tonic_midi(spec_.key);
  int top_degree = bottom_degree + size;
  std::pair<int, int> midi_range = {tonic_midi, tonic_midi + 12};

  auto candidates = drills::midi_candidates_for_degree(spec_.key, bottom_degree, midi_range);
  int bottom_midi = drills::degree_to_midi(spec_, bottom_degree, midi_range);
  if (!candidates.empty()) {
    if (avoid_repetition(spec_) && last_bottom_midi_.has_value() && candidates.size() > 1) {
      candidates.erase(std::remove(candidates.begin(), candidates.end(), last_bottom_midi_.value()),
                       candidates.end());
      if (candidates.empty()) {
        candidates = drills::midi_candidates_for_degree(spec_.key, bottom_degree, midi_range);
      }
    }
    int idx = rand_int(rng_state, 0, static_cast<int>(candidates.size()) - 1);
    bottom_midi = candidates[static_cast<std::size_t>(idx)];
  }

  int semitone_diff = drills::degree_to_offset(top_degree) - drills::degree_to_offset(bottom_degree);
  int top_midi = bottom_midi + semitone_diff;

  bool ascending = rand_int(rng_state, 0, 1) == 1;
  std::string orientation = ascending ? "ascending" : "descending";

  last_bottom_degree_ = bottom_degree;
  last_bottom_midi_ = bottom_midi;

  //-----------------------------------------------------
  // PREPARE QUESTION AND ANSWER
  //-----------------------------------------------------
  int num_notes = 2;
  std::vector<int> notes = std::vector<int>(bottom_degree, top_degree);
  std::string interval_name = get_interval_name(top_midi - bottom_midi);
  
  HarmonyAnswerV2 interval_answer = HarmonyAnswerV2{notes};
  HarmonyQuestionV2 interval_question = HarmonyQuestionV2{
   tonic_midi, spec_.key, spec_.quality, num_notes, notes, interval_name
  };

  //-----------------------------------------------------------------
  // GENERATE MIDI-CLIP
  //-----------------------------------------------------------------
  MidiClipBuilder b(params.tempo_bpm, 480);
  auto melody_track = b.add_track("melody", 0, params.program);

  Beats beat = Beats{0}; // CURRENT BEAT
  std::vector<int> midis = std::vector<int>(bottom_midi, top_midi);
  for (int midi : midis){
    b.add_note(melody_track, beat, Beats{params.note_beat}, midi, params.velocity);
  }

  if (params.add_helper) {
    // ADD HELPER MELODY (BOTTOM NOTE)
    auto helper_track = b.add_track("helper", 1, 0); // PIANO
    Beats helper_beat = Beats{1.0};
    for (int midi : midis) {
      b.add_note(helper_track, helper_beat, Beats{params.note_beat}, midi, 64);
      helper_beat.advance_by(0.5);
    }
  }

  //-----------------------------------------------------------------
  // GENERATE QUESTION BUNDLE
  //-----------------------------------------------------------------
  QuestionBundle bundle;
  bundle.question_id = "place-holder";
  bundle.question_id.clear();
  bundle.correct_answer = interval_answer;
  bundle.question = interval_question;
  bundle.prompt_clip = b.build();

  return bundle;
}

} // namespace ear
