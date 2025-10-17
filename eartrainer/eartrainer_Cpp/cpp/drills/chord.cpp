#include "chord.hpp"

#include "common.hpp"
#include "../include/ear/drill_spec.hpp"
#include "../src/rng.hpp"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
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

double center_of_mass(const std::vector<int>& values) {
  if (values.empty()) {
    return 0.0;
  }
  double sum = 0.0;
  for (int v : values) {
    sum += static_cast<double>(v);
  }
  return sum / static_cast<double>(values.size());
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

std::vector<int> to_vector(const nlohmann::json& array_node) {
  std::vector<int> values;
  if (!array_node.is_array()) {
    return values;
  }
  const auto& arr = array_node.get_array();
  for (const auto& item : arr) {
    values.push_back(item.get<int>());
  }
  return values;
}

int spec_param_int(const DrillSpec& spec, const std::string& key, int fallback) {
  if (!spec.params.is_object()) {
    return fallback;
  }
  if (spec.params.contains(key)) {
    const auto& node = spec.params[key];
    if (node.is_number_integer()) {
      return node.get<int>();
    }
    if (node.is_number()) {
      return static_cast<int>(node.get<double>());
    }
  }
  return fallback;
}

bool spec_param_bool(const DrillSpec& spec, const std::string& key, bool fallback) {
  if (!spec.params.is_object()) {
    return fallback;
  }
  if (spec.params.contains(key)) {
    const auto& node = spec.params[key];
    if (node.is_boolean()) {
      return node.get<bool>();
    }
  }
  return fallback;
}

} // namespace

void ChordDrill::configure(const DrillSpec& spec) {
  spec_ = spec;
  last_degree_.reset();
  last_voicing_id_.reset();
  preferred_right_voicing_ = spec_param_string(spec_, "chord_right_voicing");
  preferred_bass_voicing_ = spec_param_string(spec_, "chord_bass_voicing");
  if (auto profile = spec_param_string(spec_, "chord_voicing_profile")) {
    voicing_source_id_ = *profile;
  } else {
    voicing_source_id_ = "builtin_diatonic_triads";
  }
}

QuestionBundle ChordDrill::next_question(std::uint64_t& rng_state) {
  int degree = pick_degree(spec_, rng_state, last_degree_);
  last_degree_ = degree;

  bool add_seventh = false;
  if (spec_.params.contains("add_seventh")) {
    add_seventh = spec_.params["add_seventh"].get<bool>();
  }

  std::string quality = chord_quality_for_degree(degree);
  auto quality_enum = triad_quality_from_string(quality);
  const auto& voicing_eng = ChordVoicingEngine::instance();

  std::optional<std::string> avoid_voicing;
  if (avoid_repetition(spec_) && last_voicing_id_.has_value()) {
    avoid_voicing = last_voicing_id_;
  }

  auto selection = voicing_eng.pick_triad(quality_enum, rng_state,
                                     preferred_right_voicing_,
                                     preferred_bass_voicing_,
                                     avoid_voicing);

  if (!selection.right_hand || !selection.bass) {
    throw std::runtime_error("ChordDrill: missing voicing definition for quality '" + quality + "'");
  }

  last_voicing_id_ = selection.right_hand->id;

  int bass_offset = selection.bass->degree_offset;
  auto payload = build_degrees_payload(degree,
                                       quality,
                                       *selection.right_hand,
                                       bass_offset,
                                       selection.right_hand->id,
                                       add_seventh);
  payload["bass_voicing_id"] = selection.bass->id;
  payload["right_voicing_id"] = selection.right_hand->id;

  int root_degree = payload["root"].get<int>();
  std::string sampled_quality = payload["quality"].get<std::string>();
  std::string sampled_voicing_id = payload["voicing_id"].get<std::string>();
  std::string sampled_bass_id = payload["bass_voicing_id"].get<std::string>();
  int bass_degree = payload["bass_degree"].get<int>();
  int sampled_bass_offset = payload["bass_offset"].get<int>();

  nlohmann::json right_offsets_json = payload["right_offsets"];
  nlohmann::json degrees_node = payload["degrees"];
  auto right_degrees = to_vector(degrees_node);
  if (right_degrees.empty()) {
    auto right_offsets = to_vector(right_offsets_json);
    for (int offset : right_offsets) {
      right_degrees.push_back(root_degree + offset);
    }
  }

  const auto& bass_options = voicing_eng.bass_options(quality_enum);
  const auto& right_options = voicing_eng.right_hand_options(quality_enum);

  int sampled_voicing_index = 0;
  for (std::size_t i = 0; i < right_options.size(); ++i) {
    if (right_options[i].id == sampled_voicing_id) {
      sampled_voicing_index = static_cast<int>(i);
      break;
    }
  }

  PromptPlan plan;
  plan.modality = "midi-clip";
  plan.tempo_bpm = spec_.tempo_bpm;
  plan.count_in = false;

  const int prompt_tempo = plan.tempo_bpm.has_value() ? plan.tempo_bpm.value() : 90;
  constexpr int kChordDurMs = 900;
  const int default_program = spec_param_int(spec_, "chord_prompt_program", 0);
  const bool split_tracks = spec_param_bool(spec_, "chord_prompt_split_tracks", true);
  const int strum_step_ms = spec_param_int(spec_, "chord_prompt_strum_step_ms", 0);
  const int default_velocity = spec_param_int(spec_, "chord_prompt_velocity", 90);

  int tonic_midi = drills::central_tonic_midi(spec_.key);
  int bass_base = tonic_midi + drills::degree_to_offset(bass_degree);
  constexpr int kBassTarget = 36; // C2
  int best_bass = bass_base;
  int best_diff = std::numeric_limits<int>::max();
  for (int k = -4; k <= 4; ++k) {
    int candidate = bass_base + 12 * k;
    if (candidate < 0 || candidate > 127) {
      continue;
    }
    int diff = std::abs(candidate - kBassTarget);
    if (diff < best_diff) {
      best_diff = diff;
      best_bass = candidate;
    }
  }
  int bass_midi = drills::clamp_to_range(best_bass, 0, 127);

  nlohmann::json midi_tones = nlohmann::json::array();
  nlohmann::json realised_degrees = nlohmann::json::array();
  midi_tones.push_back(bass_midi);
  realised_degrees.push_back(bass_degree);
  plan.notes.push_back({bass_midi, kChordDurMs, std::nullopt, std::nullopt});

  std::vector<int> base_right_midi;
  base_right_midi.reserve(right_degrees.size());
  for (int degree_value : right_degrees) {
    base_right_midi.push_back(tonic_midi + drills::degree_to_offset(degree_value));
  }

  constexpr int kRightTarget = 60; // C4
  std::vector<int> chosen_right = base_right_midi;
  double best_distance = std::numeric_limits<double>::infinity();
  for (int k = -1; k <= 1; ++k) {
    std::vector<int> candidate;
    candidate.reserve(base_right_midi.size());
    for (int value : base_right_midi) {
      candidate.push_back(value + 12 * k);
    }
    double distance = std::abs(center_of_mass(candidate) - static_cast<double>(kRightTarget));
    if (distance < best_distance) {
      best_distance = distance;
      chosen_right = candidate;
    }
  }

  std::vector<int> right_midi;
  right_midi.reserve(chosen_right.size());
  int previous = bass_midi;
  for (std::size_t i = 0; i < chosen_right.size(); ++i) {
    int midi = chosen_right[i];
    while (midi <= previous) {
      midi += 12;
    }
    midi = drills::clamp_to_range(midi, 0, 127);
    right_midi.push_back(midi);
    realised_degrees.push_back(right_degrees[i]);
    midi_tones.push_back(midi);
    plan.notes.push_back({midi, kChordDurMs, std::nullopt, std::nullopt});
    previous = midi;
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
  manifest["add_seventh"] = add_seventh;
  manifest["quality"] = sampled_quality;
  manifest["voicing_id"] = sampled_voicing_id;
  manifest["voicing_index"] = sampled_voicing_index;
  manifest["selected_bass_offset"] = sampled_bass_offset;
  manifest["bass_voicing_id"] = sampled_bass_id;
  manifest["bass_degree"] = bass_degree;
  manifest["root_degree"] = root_degree;
  manifest["right_offsets"] = right_offsets_json;
  manifest["right_voicing_id"] = sampled_voicing_id;
  manifest["voicings_source"] = voicing_source_id_;
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
    const int bass_channel = spec_param_int(spec_, "chord_prompt_bass_channel", 1);
    const int bass_program = spec_param_int(spec_, "chord_prompt_bass_program", default_program);
    auto bass_track = make_track("bass", bass_channel, bass_program);
    bass_track["notes"].push_back(make_note(bass_midi, 0, kChordDurMs, default_velocity));

    const int right_channel = spec_param_int(spec_, "chord_prompt_right_channel", 0);
    const int right_program = spec_param_int(spec_, "chord_prompt_right_program", default_program);
    auto right_track = make_track("right", right_channel, right_program);
    for (std::size_t i = 0; i < right_midi.size(); ++i) {
      int onset_ms = strum_step_ms > 0 ? static_cast<int>(i) * strum_step_ms : 0;
      right_track["notes"].push_back(make_note(right_midi[i], onset_ms, kChordDurMs, default_velocity));
    }

    tracks.push_back(std::move(bass_track));
    tracks.push_back(std::move(right_track));
  } else {
    const int merged_channel = spec_param_int(spec_, "chord_prompt_channel", 0);
    auto prompt_track = make_track("prompt", merged_channel, default_program);
    prompt_track["notes"].push_back(make_note(bass_midi, 0, kChordDurMs, default_velocity));
    for (std::size_t i = 0; i < right_midi.size(); ++i) {
      int onset_ms = strum_step_ms > 0 ? static_cast<int>(i) * strum_step_ms : 0;
      prompt_track["notes"].push_back(make_note(right_midi[i], onset_ms, kChordDurMs, default_velocity));
    }
    tracks.push_back(std::move(prompt_track));
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
  question_payload["voicings_source"] = voicing_source_id_;

  nlohmann::json answer_payload = nlohmann::json::object();
  answer_payload["root_degree"] = root_degree;

  nlohmann::json hints = nlohmann::json::object();
  hints["prompt_manifest"] = manifest;
  hints["answer_kind"] = "chord_degree";
  hints["right_voicing_id"] = sampled_voicing_id;
  hints["bass_voicing_id"] = sampled_bass_id;
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
