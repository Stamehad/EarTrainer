#include "chord.hpp"

#include "common.hpp"
#include "params.hpp"
#include "prompt_utils.hpp"
#include "chord_core.hpp"
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

bool avoid_repetition(const DrillSpec& spec) {
  if (!spec.j_params.is_object() || !spec.j_params.contains("chord_avoid_repeat")) {
    if (!spec.j_params.is_object() || !spec.j_params.contains("avoid_repeat")) {
      return true;
    }
    return spec.j_params["avoid_repeat"].get<bool>();
  }
  return spec.j_params["chord_avoid_repeat"].get<bool>();
}

int pick_degree(const ChordParams& params, std::uint64_t& rng_state,
                const std::optional<int>& previous) {
  auto allowed = params.allowed_degrees;

  if (params.avoid_repeat && previous.has_value() && allowed.size() > 1) {
    allowed.erase(std::remove(allowed.begin(), allowed.end(), previous.value()), allowed.end());
    if (allowed.empty()) {
      allowed = params.allowed_degrees;
    }
  }

  int idx = rand_int(rng_state, 0, static_cast<int>(allowed.size()) - 1);
  return allowed[static_cast<std::size_t>(idx)];
}

std::string chord_quality_for_degree(int degree) {
  static const std::unordered_map<int, std::string> mapping = {
      {0, "major"}, {1, "minor"},      {2, "minor"},
      {3, "major"}, {4, "major"},      {5, "minor"},
      {6, "diminished"}};
  int idx = degree % 7;
  if (idx < 0) {
    idx += 7;
  }
  auto it = mapping.find(idx);
  if (it != mapping.end()) {
    return it->second;
  }
  return "major";
}

std::optional<std::string> spec_param_string(const DrillSpec& spec, const std::string& key) {
  if (!spec.j_params.is_object()) {
    return std::nullopt;
  }
  if (!spec.j_params.contains(key)) {
    return std::nullopt;
  }
  const auto& node = spec.j_params[key];
  if (!node.is_string()) {
    return std::nullopt;
  }
  std::string value = node.get<std::string>();
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

nlohmann::json build_degrees_payload(int root_degree,
                                     const std::string& quality,
                                     const ChordVoicingEngine::RightHandPattern& right_pattern,
                                     int bass_offset,
                                     const std::string& right_voicing_id,
                                     bool add_seventh = false) {
  const auto& relative_offsets = right_pattern.degree_offsets;

  nlohmann::json right_offsets_json = nlohmann::json::array();
  nlohmann::json right_degrees_json = nlohmann::json::array();
  for (int offset : relative_offsets) {
    right_offsets_json.push_back(offset);
    right_degrees_json.push_back(root_degree + offset);
  }

  nlohmann::json payload = nlohmann::json::object();
  payload["root"] = root_degree;
  payload["degrees"] = right_degrees_json;
  payload["quality"] = quality;
  payload["voicing_id"] = right_voicing_id;
  payload["add_seventh"] = add_seventh;
  payload["bass_offset"] = bass_offset;
  payload["bass_degree"] = root_degree + bass_offset;
  payload["right_offsets"] = right_offsets_json;
  return payload;
}

} // namespace

//====================================================================
// INITIALIZE CHORD DRILL
//====================================================================
void ChordDrill::configure(const DrillSpec& spec) {
  spec_ = spec;
  params = std::get<ChordParams>(spec_.params);
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
  v_engine.configure(spec_.quality, params.inst, tonic_midi, params.voice_leading_continuity);
}

//====================================================================
// NEXT QUESTION -> QUESTION BUNDLE
//====================================================================
QuestionsBundle ChordDrill::next_question(std::uint64_t& rng_state) {
  // SAMPLE DEGREE -> VOICING + BASS
  int degree = pick_degree(params, rng_state, selection_state_.last_degree);
  ChordVoicingEngine::RightVoicing rv = v_engine.get_voicing(degree, rng_state);
  ChordVoicingEngine::BassChoice bass = v_engine.get_bass(degree, rng_state);
  
  //-----------------------------------------------------------------
  // PREPARE QUESTION AND ANSWER
  //-----------------------------------------------------------------
  ear::ChordAnswerV2 chord_answer = ear::ChordAnswerV2{degree};
  ear::ChordQuestionV2 chord_question = ear::ChordQuestionV2{
   tonic_midi, spec_.key, spec_.quality, degree, rv.quality, 
   rv.degree_offsets, bass.bass_degree, rv.id, bass.id
  };

  //-----------------------------------------------------------------
  // GENERATE MIDI-CLIP
  //-----------------------------------------------------------------
  MidiClipBuilder b(params.tempo_bpm, 480);
  auto right_track = b.add_track("right", params.right_channel, params.right_program);
  auto bass_track  = b.add_track("bass",  params.bass_channel, params.bass_program);

  
  Beats beat = Beats{0.0};
  b.add_chord(right_track, beat, Beats{params.dur_beats}, rv.right_midi, params.velocity);
  b.add_note(bass_track, beat, Beats{params.dur_beats}, bass.bass_midi,  params.velocity);

  if (params.training_root.enabled) {
    auto helper_track = b.add_track("helper", params.training_root.channel, params.training_root.program);
    // ROOT HELPER
    int root_midi = tonic_midi + drills::degree_to_offset(degree);
    beat.advance_by(params.training_root.delay_beats);
    b.add_note(helper_track, beat, Beats{params.training_root.dur_beats}, root_midi, params.velocity);
  }

  //-----------------------------------------------------------------
  // GENERATE QUESTION BUNDLE
  //-----------------------------------------------------------------
  ear::QuestionsBundle bundle;
  bundle.question_id = "place-holder";
  bundle.question_id.clear();
  bundle.correct_answer = chord_answer;
  bundle.question = chord_question;
  bundle.prompt_clip = b.build();

  return bundle;
}

} // namespace ear
