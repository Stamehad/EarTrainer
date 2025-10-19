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
  if (!spec.params.is_object() || !spec.params.contains("chord_avoid_repeat")) {
    if (!spec.params.is_object() || !spec.params.contains("avoid_repeat")) {
      return true;
    }
    return spec.params["avoid_repeat"].get<bool>();
  }
  return spec.params["chord_avoid_repeat"].get<bool>();
}

int pick_degree(const DrillSpec& spec, std::uint64_t& rng_state,
                const std::optional<int>& previous) {
  auto allowed = extract_allowed(spec, "chord_allowed_degrees");
  if (allowed.empty()) {
    allowed = extract_allowed(spec, "allowed_degrees");
  }
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
  if (!spec.params.is_object()) {
    return std::nullopt;
  }
  if (!spec.params.contains(key)) {
    return std::nullopt;
  }
  const auto& node = spec.params[key];
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

void ChordDrill::configure(const DrillSpec& spec) {
  spec_ = spec;
  selection_state_ = {};
  preferred_right_voicing_ = spec_param_string(spec_, "chord_right_voicing");
  preferred_bass_voicing_ = spec_param_string(spec_, "chord_bass_voicing");
  if (auto profile = spec_param_string(spec_, "chord_voicing_profile")) {
    voicing_source_id_ = ChordVoicingEngine::instance().resolve_profile_id(*profile);
  } else {
    voicing_source_id_ = ChordVoicingEngine::default_profile_id();
  }
}

void SustainChordDrill::configure(const DrillSpec& spec) {
  DrillSpec adjusted = spec;
  drills::ensure_object(adjusted.params);

  auto& params = adjusted.params;
  if (!params.contains("chord_voicing_profile")) {
    params["chord_voicing_profile"] = "strings_ensemble";
  }

  const int kDefaultStringsProgram = 48; // General MIDI: Strings Ensemble 1
  if (!params.contains("chord_prompt_split_tracks")) {
    params["chord_prompt_split_tracks"] = false;
  }
  if (!params.contains("chord_prompt_program")
      && !params.contains("chord_prompt_right_program")
      && !params.contains("chord_prompt_bass_program")) {
    params["chord_prompt_program"] = kDefaultStringsProgram;
  }
  if (!params.contains("chord_prompt_channel")) {
    params["chord_prompt_channel"] = 0;
  }
  if (!params.contains("chord_prompt_velocity")) {
    params["chord_prompt_velocity"] = 96;
  }
  if (!params.contains("chord_prompt_duration_ms")) {
    params["chord_prompt_duration_ms"] = 10000;
  }
  if (!params.contains("chord_voice_leading_continuity")) {
    params["chord_voice_leading_continuity"] = true;
  }

  ChordDrill::configure(adjusted);
}

QuestionBundle ChordDrill::next_question(std::uint64_t& rng_state) {
  int degree = pick_degree(spec_, rng_state, selection_state_.last_degree);

  auto core = drills::chord::prepare_chord_question(spec_, degree, rng_state, selection_state_,
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
  plan.tempo_bpm = spec_.tempo_bpm;
  plan.count_in = false;

  const int prompt_tempo = plan.tempo_bpm.has_value() ? plan.tempo_bpm.value() : 90;
  int chord_dur_ms = drills::param_int(spec_, "chord_prompt_duration_ms", 900);
  if (chord_dur_ms <= 0) {
    chord_dur_ms = 900;
  }
  const int default_program = drills::param_int(spec_, "chord_prompt_program", 0);
  const bool split_tracks = drills::param_flag(spec_, "chord_prompt_split_tracks", true);
  const int strum_step_ms = drills::param_int(spec_, "chord_prompt_strum_step_ms", 0);
  const int default_velocity = drills::param_int(spec_, "chord_prompt_velocity", 90);

  auto helper_config = drills::chord::resolve_training_root_config(
      spec_, split_tracks, chord_dur_ms, default_velocity,
      drills::param_int(spec_, "chord_prompt_right_channel", 0),
      drills::param_int(spec_, "chord_prompt_channel", 0));
  int helper_start_ms = drills::beats_to_ms(helper_config.delay_beats, prompt_tempo);

  int tonic_midi = core.tonic_midi;
  int bass_midi = drills::chord::select_bass_midi(core);
  int helper_midi = drills::degree_to_midi(spec_, core.root_degree);

  nlohmann::json midi_tones = nlohmann::json::array();
  nlohmann::json realised_degrees = nlohmann::json::array();
  midi_tones.push_back(bass_midi);
  realised_degrees.push_back(bass_degree);
  plan.notes.push_back({bass_midi, chord_dur_ms, std::nullopt, std::nullopt});

  std::vector<int> right_midi = drills::chord::voice_right_hand_midi(core, bass_midi);
  for (std::size_t i = 0; i < right_midi.size(); ++i) {
    realised_degrees.push_back(right_degrees[i]);
    midi_tones.push_back(right_midi[i]);
    plan.notes.push_back({right_midi[i], chord_dur_ms, std::nullopt, std::nullopt});
  }

  if (helper_config.enabled) {
    helper_midi = drills::chord::adjust_helper_midi(helper_midi, bass_midi, right_midi);
    drills::append_note(plan, helper_midi, helper_config.duration_ms, helper_config.velocity);
  }

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
    const int bass_channel = drills::param_int(spec_, "chord_prompt_bass_channel", 1);
    const int bass_program = drills::param_int(spec_, "chord_prompt_bass_program", default_program);
    auto bass_track = make_track("bass", bass_channel, bass_program);
    bass_track["notes"].push_back(make_note(bass_midi, 0, chord_dur_ms, default_velocity));

    const int right_channel = drills::param_int(spec_, "chord_prompt_right_channel", 0);
    const int right_program = drills::param_int(spec_, "chord_prompt_right_program", default_program);
    auto right_track = make_track("right", right_channel, right_program);
    for (std::size_t i = 0; i < right_midi.size(); ++i) {
      int onset_ms = strum_step_ms > 0 ? static_cast<int>(i) * strum_step_ms : 0;
      right_track["notes"].push_back(make_note(right_midi[i], onset_ms, chord_dur_ms, default_velocity));
    }

    tracks.push_back(std::move(bass_track));
    tracks.push_back(std::move(right_track));
  } else {
    const int merged_channel = drills::param_int(spec_, "chord_prompt_channel", 0);
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
