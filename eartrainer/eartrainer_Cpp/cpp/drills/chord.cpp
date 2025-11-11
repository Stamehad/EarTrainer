#include "chord.hpp"

#include "common.hpp"
#include "params.hpp"
#include "prompt_utils.hpp"
#include "../include/ear/drill_spec.hpp"
#include "../src/rng.hpp"
#include "../include/ear/question_bundle_v2.hpp"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ear {

namespace {

struct SequenceChord {
  int degree = 0;
  bool is_anchor = false;
  bool answerable = true;
};

int sample_length(const std::vector<int>& lengths, std::uint64_t& rng_state) {
  std::vector<int> positive;
  positive.reserve(lengths.size());
  for (int len : lengths) {
    if (len > 0) {
      positive.push_back(std::clamp(len, 1, 4));
    }
  }
  if (positive.empty()) {
    positive.push_back(1);
  }
  int idx = rand_int(rng_state, 0, static_cast<int>(positive.size()) - 1);
  return positive[static_cast<std::size_t>(idx)];
}

std::vector<int> default_degree_pool() {
  return {0, 1, 2, 3, 4, 5, 6};
}

std::vector<int> sample_progression_degrees(const ChordParams& params,
                                            int length,
                                            std::uint64_t& rng_state,
                                            const std::optional<int>& previous_degree) {
  std::vector<int> base = params.degrees.empty() ? default_degree_pool() : params.degrees;
  std::vector<int> result;
  result.reserve(static_cast<std::size_t>(std::max(1, length)));

  std::vector<int> available = base;
  auto rebuild_available = [&]() {
    available = base;
    for (int used : result) {
      auto it = std::find(available.begin(), available.end(), used);
      if (it != available.end()) {
        available.erase(it);
      }
    }
  };

  for (int i = 0; i < length; ++i) {
    if (available.empty()) {
      rebuild_available();
      if (available.empty()) {
        available = base; // allow repeats if we ran out of unique options
      }
    }

    auto candidates = available;
    if (i == 0 && params.avoid_repeat && previous_degree.has_value() && candidates.size() > 1) {
      candidates.erase(std::remove(candidates.begin(), candidates.end(), previous_degree.value()),
                       candidates.end());
      if (candidates.empty()) {
        candidates = available;
      }
    }
    if (candidates.empty()) {
      candidates = base.empty() ? default_degree_pool() : base;
    }

    int idx = rand_int(rng_state, 0, static_cast<int>(candidates.size()) - 1);
    int pick = candidates[static_cast<std::size_t>(idx)];
    result.push_back(pick);
    available.erase(std::remove(available.begin(), available.end(), pick), available.end());
  }

  return result;
}

std::pair<bool, bool> anchor_positions(const ChordParams& params, std::uint64_t& rng_state) {
  if (!params.use_anchor) {
    return {false, false};
  }
  if (params.tonic_anchor.has_value()) {
    bool before = params.tonic_anchor.value() == ChordParams::TonicAnchor::Before;
    bool after = params.tonic_anchor.value() == ChordParams::TonicAnchor::After;
    return {before, after};
  }
  bool before = rand_int(rng_state, 0, 1) == 0;
  return {before, !before};
}

std::vector<SequenceChord> build_sequence(const std::vector<int>& progression,
                                          std::pair<bool, bool> anchor_mode) {
  std::vector<SequenceChord> seq;
  seq.reserve(progression.size() + (anchor_mode.first ? 1 : 0) + (anchor_mode.second ? 1 : 0));
  if (anchor_mode.first) {
    seq.push_back(SequenceChord{0, true, false});
  }
  for (int deg : progression) {
    seq.push_back(SequenceChord{deg, false, true});
  }
  if (anchor_mode.second) {
    seq.push_back(SequenceChord{0, true, false});
  }
  return seq;
}

int compute_top_degree(int root_degree, const ChordVoicingEngine::RightVoicing& rv) {
  if (rv.degree_offsets.empty()) {
    return drills::normalize_degree_index(root_degree);
  }
  int top = root_degree + rv.degree_offsets.back();
  return drills::normalize_degree_index(top);
}

std::vector<int> absolute_degrees(int root_degree, const std::vector<int>& offsets) {
  std::vector<int> values;
  values.reserve(offsets.size());
  for (int offset : offsets) {
    values.push_back(root_degree + offset);
  }
  return values;
}


} // namespace

//====================================================================
// INITIALIZE CHORD DRILL
//====================================================================
void ChordDrill::configure(const DrillSpec& spec) {
  spec_ = spec;
  try {
    params = std::get<ChordParams>(spec_.params);
  } catch (const std::bad_variant_access&) {
    throw std::runtime_error("ChordDrill: spec '" + spec_.id + "' missing chord params");
  }
  if (params.use_anchor && params.play_root.enabled) {
    // ensure only one helper (anchor or training root) is active at a time
    params.play_root.enabled = false;
  }
  tonic_midi = drills::central_tonic_midi(spec_.key);
  inst = params.inst;
  selection_state_ = {};
  preferred_right_voicing_ = params.right_voicing_id;
  preferred_bass_voicing_ = params.bass_voicing_id;
  if (auto profile = params.voicing_profile) {
    voicing_source_id_ = ChordVoicingEngine::instance().resolve_profile_id(*profile);
  } else {
    voicing_source_id_ = ChordVoicingEngine::default_profile_id();
  }

  auto& v_engine = ear::ChordVoicingEngine::instance();  
  v_engine.configure(spec_.quality, params.inst, params.voicing_style, tonic_midi, params.voice_leading_continuity);
}

//====================================================================
// NEXT QUESTION -> QUESTION BUNDLE
//====================================================================
QuestionBundle ChordDrill::next_question(std::uint64_t& rng_state) {
  int progression_length = sample_length(params.sequence_lengths, rng_state);
  auto progression_degrees = sample_progression_degrees(params, progression_length, rng_state,
                                                       selection_state_.last_degree);
  if (!progression_degrees.empty()) {
    selection_state_.last_degree = progression_degrees.back();
  }

  auto anchors = anchor_positions(params, rng_state);
  auto sequence = build_sequence(progression_degrees, anchors);
  if (sequence.empty()) {
    sequence.push_back(SequenceChord{0, false, true});
  }

  struct VoicedChord {
    SequenceChord meta;
    ChordVoicingEngine::RightVoicing right;
    ChordVoicingEngine::BassChoice bass;
  };

  std::vector<VoicedChord> voiced;
  voiced.reserve(sequence.size());
  for (const auto& entry : sequence) {
    auto rv = v_engine.get_voicing(entry.degree, rng_state);
    bool allow_inversions = params.sample_inversions && !entry.is_anchor;
    auto bass = v_engine.get_bass(entry.degree, allow_inversions, rng_state);
    voiced.push_back(VoicedChord{entry, rv, bass});
  }

  std::size_t chord_count = voiced.size();

  ChordQuestionV2 chord_question{};
  chord_question.tonic_midi = tonic_midi;
  chord_question.tonic = spec_.key;
  chord_question.key = spec_.quality;
  chord_question.root_degrees.resize(chord_count);
  chord_question.qualities.resize(chord_count);
  chord_question.rh_degrees.resize(chord_count);
  chord_question.bass_degrees.resize(chord_count);
  chord_question.right_voicing_ids.resize(chord_count);
  chord_question.bass_voicing_ids.resize(chord_count);
  chord_question.is_anchor.resize(chord_count);

  ChordAnswerV2 chord_answer{};
  chord_answer.root_degrees.resize(chord_count);
  chord_answer.bass_deg.resize(chord_count);
  chord_answer.top_deg.resize(chord_count);
  chord_answer.expect_root.assign(chord_count, false);
  chord_answer.expect_bass.assign(chord_count, false);
  chord_answer.expect_top.assign(chord_count, false);

  for (std::size_t i = 0; i < chord_count; ++i) {
    const auto& chord = voiced[i];
    chord_question.root_degrees[i] = chord.meta.degree;
    chord_question.qualities[i] = chord.right.quality;
    chord_question.rh_degrees[i] = absolute_degrees(chord.meta.degree, chord.right.degree_offsets);
    chord_question.bass_degrees[i] = chord.bass.bass_degree;
    chord_question.right_voicing_ids[i] = chord.right.id;
    chord_question.bass_voicing_ids[i] = chord.bass.id;
    chord_question.is_anchor[i] = chord.meta.is_anchor;

    chord_answer.root_degrees[i] = chord.meta.degree;
    chord_answer.bass_deg[i] = drills::normalize_degree_index(chord.bass.bass_degree);
    chord_answer.top_deg[i] = compute_top_degree(chord.meta.degree, chord.right);
    chord_answer.expect_root[i] = chord.meta.answerable;
    chord_answer.expect_top[i] = chord.meta.answerable;
    chord_answer.expect_bass[i] = chord.meta.answerable && params.sample_inversions;
  }

  //-----------------------------------------------------------------
  // GENERATE MIDI-CLIP
  //-----------------------------------------------------------------
  int right_program;
  int bass_program;
  switch (inst) {
    case DrillInstrument::Piano:
      right_program = 0; bass_program = 0; break;
    default:
      right_program = 48; bass_program = 48;
  }
  MidiClipBuilder b(params.bpm, 480);
  auto right_track = b.add_track("right", params.right_channel, right_program);
  auto bass_track  = b.add_track("bass",  params.bass_channel, bass_program);

  Beats beat = Beats{0.0};
  for (const auto& chord : voiced) {
    b.add_chord(right_track, beat, Beats{params.dur_beats}, chord.right.right_midi, params.velocity);
    bool include_bass = (params.voicing_style != VoicingsStyle::Triad) || params.sample_inversions || chord.meta.is_anchor;
    if (include_bass) {
      b.add_note(bass_track, beat, Beats{params.dur_beats}, chord.bass.bass_midi, params.velocity);
    }
    beat.advance_by(params.dur_beats);
  }

  // Training root helper (only when not using tonic anchor)
  if (params.play_root.enabled && !progression_degrees.empty()) {
    auto helper_track = b.add_track("helper", params.play_root.channel, params.play_root.program);
    int helper_velocity = params.play_root.velocity > 0 ? params.play_root.velocity : params.velocity;
    int root_midi = tonic_midi + drills::degree_to_offset(progression_degrees.front());
    Beats helper_start = Beats{params.play_root.delay_beats};
    b.add_note(helper_track, helper_start, Beats{params.play_root.dur_beats}, root_midi, helper_velocity);
  }

  //-----------------------------------------------------------------
  // GENERATE QUESTION BUNDLE
  //-----------------------------------------------------------------
  ear::QuestionBundle bundle;
  bundle.question_id.clear();
  bundle.correct_answer = chord_answer;
  bundle.question = chord_question;
  bundle.prompt_clip = b.build();

  return bundle;
}

} // namespace ear
