#include "chord.hpp"

#include "common.hpp"
#include "params.hpp"
#include "prompt_utils.hpp"
#include "chord_core.hpp"
#include "../include/ear/drill_spec.hpp"
#include "../src/rng.hpp"

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
  v_engine.configure(KeyType::Major, params.inst, tonic_midi, params.voice_leading_continuity);
}

//====================================================================
// NEXT QUESTION -> QUESTION BUNDLE
//====================================================================
QuestionBundle ChordDrill::next_question(std::uint64_t& rng_state) {
  int degree = pick_degree(params, rng_state, selection_state_.last_degree);
  ChordVoicingEngine::RightVoicing rv = v_engine.get_voicing(degree, rng_state);
  ChordVoicingEngine::BassChoice bass = v_engine.get_bass(degree, rng_state);

  auto core = drills::chord::prepare_chord_question(params, spec_.key, degree, rng_state, selection_state_,
                                                    preferred_right_voicing_,
                                                    preferred_bass_voicing_, voicing_source_id_);
  const auto& voicing_eng = ChordVoicingEngine::instance();
  
  auto payload = build_degrees_payload(core.root_degree,
                                       core.quality,
                                       *core.right_pattern,
                                       core.bass_offset,
                                       core.right_voicing_id,
                                       core.add_seventh);
  payload["bass_voicing_id"] = core.bass_voicing_id;
  payload["right_voicing_id"] = core.right_voicing_id;

  int root_degree = core.root_degree;
  std::string sampled_quality = core.quality;
  std::string sampled_voicing_id = core.right_voicing_id;
  std::string sampled_bass_id = core.bass_voicing_id;
  int bass_degree = core.bass_degree;
  int sampled_bass_offset = core.bass_offset;

  nlohmann::json right_offsets_json = payload["right_offsets"];
  std::vector<int> right_degrees = core.right_degrees;

  const auto& bass_options = voicing_eng.bass_options(core.quality_enum, core.profile_id);
  const auto& right_options = voicing_eng.right_hand_options(core.quality_enum, core.profile_id);
  int sampled_voicing_index = drills::chord::find_voicing_index(right_options, sampled_voicing_id);

  PromptPlan plan;
  plan.modality = "midi-clip";
  //plan.tempo_bpm = params.tempo_bpm;
  plan.count_in = false;

  const int prompt_tempo = plan.tempo_bpm.has_value() ? plan.tempo_bpm.value() : 90;
  int chord_dur_ms = params.prompt_duration_ms; 
  if (chord_dur_ms <= 0) {
    chord_dur_ms = 900;
  }
  const int default_program = params.prompt_program; 
  const bool split_tracks = params.prompt_split_tracks; 
  const int strum_step_ms = params.prompt_strum_step_ms;
  const int default_velocity = params.prompt_velocity;


  //int tonic_midi = core.tonic_midi;
  int bass_midi = drills::chord::select_bass_midi(core);

  nlohmann::json midi_tones = nlohmann::json::array();
  nlohmann::json realised_degrees = nlohmann::json::array();
  midi_tones.push_back(bass_midi);
  realised_degrees.push_back(bass_degree);
  plan.notes.push_back({bass_midi, chord_dur_ms, std::nullopt, std::nullopt});

  std::vector<int> right_midi = drills::chord::voice_right_hand_midi(
      params, core, bass_midi, selection_state_.last_top_midi);
  for (std::size_t i = 0; i < right_midi.size(); ++i) {
    realised_degrees.push_back(right_degrees[i]);
    midi_tones.push_back(right_midi[i]);
    plan.notes.push_back({right_midi[i], chord_dur_ms, std::nullopt, std::nullopt});
  }

  if (!right_midi.empty()) {
    selection_state_.last_top_midi = right_midi.back();
  } else {
    selection_state_.last_top_midi.reset();
  }

  //----------------------------------------------------------------
  // ADD CHORD ROOT EMPHASIS AS TRAINING WHEELS
  //----------------------------------------------------------------
  auto helper_config = params.training_root;
  int helper_start_ms = drills::beats_to_ms(helper_config.delay_beats, prompt_tempo); 
  int helper_midi = tonic_midi + drills::degree_to_offset(degree);
  helper_midi = drills::chord::adjust_helper_midi(helper_midi, bass_midi, right_midi);
  drills::append_note(plan, helper_midi, helper_config.duration_ms, helper_config.velocity);
  

  auto make_track = [](const std::string& name, int channel, int program) {
    nlohmann::json track = nlohmann::json::object();
    track["name"] = name;
    track["channel"] = channel;
    track["program"] = program;
    track["notes"] = nlohmann::json::array();
    return track;
  };

  auto make_note = [](int midi, int start_ms, int dur_ms, int velocity) {
    nlohmann::json note = nlohmann::json::object();
    note["midi"] = midi;
    note["start_ms"] = start_ms;
    note["dur_ms"] = dur_ms;
    note["velocity"] = velocity;
    return note;
  };

  nlohmann::json manifest = nlohmann::json::object();
  manifest["format"] = "multi_track_prompt/v1";
  manifest["tempo_bpm"] = prompt_tempo;
  manifest["ppq"] = 480;
  manifest["add_seventh"] = core.add_seventh;
  manifest["quality"] = sampled_quality;
  manifest["voicing_id"] = sampled_voicing_id;
  manifest["voicing_index"] = sampled_voicing_index;
  manifest["selected_bass_offset"] = sampled_bass_offset;
  manifest["bass_voicing_id"] = sampled_bass_id;
  manifest["bass_degree"] = bass_degree;
  manifest["root_degree"] = root_degree;
  manifest["right_offsets"] = right_offsets_json;
  manifest["right_voicing_id"] = sampled_voicing_id;
  manifest["voicings_source"] = core.profile_id;
  nlohmann::json bass_offsets_json = nlohmann::json::array();
  nlohmann::json bass_voicings_json = nlohmann::json::array();
  for (const auto& option : bass_options) {
    bass_offsets_json.push_back(option.degree_offset);
    nlohmann::json entry = nlohmann::json::object();
    entry["id"] = option.id;
    entry["offset"] = option.degree_offset;
    bass_voicings_json.push_back(entry);
  }
  manifest["available_bass_offsets"] = bass_offsets_json;
  manifest["available_bass_voicings"] = bass_voicings_json;
  nlohmann::json right_voicings_json = nlohmann::json::array();
  for (const auto& option : right_options) {
    nlohmann::json entry = nlohmann::json::object();
    entry["id"] = option.id;
    nlohmann::json offsets = nlohmann::json::array();
    for (int offset : option.degree_offsets) {
      offsets.push_back(offset);
    }
    entry["offsets"] = std::move(offsets);
    right_voicings_json.push_back(std::move(entry));
  }
  manifest["available_right_voicings"] = right_voicings_json;
  nlohmann::json tracks = nlohmann::json::array();

  if (split_tracks) {
    const int bass_channel = params.prompt_bass_channel;
    const int bass_program = params.prompt_bass_program;
    auto bass_track = make_track("bass", bass_channel, bass_program);
    bass_track["notes"].push_back(make_note(bass_midi, 0, chord_dur_ms, default_velocity));

    const int right_channel = params.prompt_right_channel;
    const int right_program = params.prompt_right_program;
    auto right_track = make_track("right", right_channel, right_program);
    for (std::size_t i = 0; i < right_midi.size(); ++i) {
      int onset_ms = strum_step_ms > 0 ? static_cast<int>(i) * strum_step_ms : 0;
      right_track["notes"].push_back(make_note(right_midi[i], onset_ms, chord_dur_ms, default_velocity));
    }

    tracks.push_back(std::move(bass_track));
    tracks.push_back(std::move(right_track));
  } else {
    const int merged_channel = params.prompt_channel;
    auto prompt_track = make_track("prompt", merged_channel, default_program);
    prompt_track["notes"].push_back(make_note(bass_midi, 0, chord_dur_ms, default_velocity));
    for (std::size_t i = 0; i < right_midi.size(); ++i) {
      int onset_ms = strum_step_ms > 0 ? static_cast<int>(i) * strum_step_ms : 0;
      prompt_track["notes"].push_back(make_note(right_midi[i], onset_ms, chord_dur_ms, default_velocity));
    }
    tracks.push_back(std::move(prompt_track));
  }

  if (helper_config.enabled) {
    auto helper_track = make_track("training_root", helper_config.channel, helper_config.program);
    helper_track["notes"].push_back(
        make_note(helper_midi, helper_start_ms, helper_config.duration_ms, helper_config.velocity));
    tracks.push_back(std::move(helper_track));

    nlohmann::json helper_manifest = nlohmann::json::object();
    helper_manifest["midi"] = helper_midi;
    helper_manifest["channel"] = helper_config.channel;
    helper_manifest["program"] = helper_config.program;
    helper_manifest["start_ms"] = helper_start_ms;
    helper_manifest["duration_ms"] = helper_config.duration_ms;
    helper_manifest["velocity"] = helper_config.velocity;
    helper_manifest["overlaps_chord"] = true;
    manifest["training_root"] = helper_manifest;
  }

  manifest["strum_step_ms"] = strum_step_ms;
  manifest["tracks"] = std::move(tracks);
  plan.midi_clip.reset();

  nlohmann::json question_payload = nlohmann::json::object();
  question_payload["root_degree"] = root_degree;
  question_payload["quality"] = sampled_quality;
  question_payload["degrees"] = realised_degrees;
  question_payload["voicing_midi"] = midi_tones;
  question_payload["tonic_midi"] = tonic_midi;
  question_payload["voicing_index"] = sampled_voicing_index;
  question_payload["voicing_id"] = sampled_voicing_id;
  question_payload["bass_midi"] = bass_midi;
  nlohmann::json right_midi_json = nlohmann::json::array();
  for (int value : right_midi) {
    right_midi_json.push_back(value);
  }
  question_payload["right_hand_midi"] = right_midi_json;
  question_payload["bass_degree"] = bass_degree;
  question_payload["bass_offset"] = sampled_bass_offset;
  question_payload["bass_voicing_id"] = sampled_bass_id;
  question_payload["right_offsets"] = right_offsets_json;
  question_payload["voicings_source"] = core.profile_id;
  if (helper_config.enabled) {
    question_payload["training_root_midi"] = helper_midi;
  }

  nlohmann::json answer_payload = nlohmann::json::object();
  answer_payload["root_degree"] = root_degree;

  nlohmann::json hints = nlohmann::json::object();
  hints["prompt_manifest"] = manifest;
  hints["answer_kind"] = "chord_degree";
  hints["right_voicing_id"] = sampled_voicing_id;
  hints["bass_voicing_id"] = sampled_bass_id;
  if (helper_config.enabled) {
    hints["training_root_available"] = true;
  }
  nlohmann::json allowed = nlohmann::json::array();
  allowed.push_back("Replay");
  allowed.push_back("GuideTone");
  hints["allowed_assists"] = allowed;
  nlohmann::json budget = nlohmann::json::object();
  for (const auto& entry : spec_.assistance_policy) {
    budget[entry.first] = entry.second;
  }
  hints["assist_budget"] = budget;

  QuestionBundle bundle;
  bundle.question_id.clear();
  bundle.question = TypedPayload{"chord", question_payload};
  bundle.correct_answer = TypedPayload{"chord_degree", answer_payload};
  bundle.prompt = plan;
  bundle.ui_hints = hints;
  return bundle;
}

} // namespace ear
