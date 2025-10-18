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

void ensure_params_object(nlohmann::json& node) {
  if (!node.is_object()) {
    node = nlohmann::json::object();
  }
}

std::vector<int> spec_param_degree_list(const DrillSpec& spec, const std::string& key) {
  std::vector<int> values;
  if (!spec.params.is_object() || !spec.params.contains(key)) {
    return values;
  }
  const auto& node = spec.params[key];
  if (!node.is_array()) {
    return values;
  }
  for (const auto& entry : node.get_array()) {
    if (entry.is_number_integer()) {
      values.push_back(drills::normalize_degree_index(entry.get<int>()));
    }
  }
  std::sort(values.begin(), values.end());
  values.erase(std::unique(values.begin(), values.end()), values.end());
  return values;
}

int pattern_top_degree(const ChordVoicingEngine::RightHandPattern& pattern, int root_degree) {
  if (pattern.degree_offsets.empty()) {
    return drills::normalize_degree_index(root_degree);
  }
  int top_offset = *std::max_element(pattern.degree_offsets.begin(), pattern.degree_offsets.end());
  return drills::normalize_degree_index(root_degree + top_offset);
}

std::vector<int> continuity_degrees(int top_degree) {
  std::vector<int> values;
  values.push_back(drills::normalize_degree_index(top_degree));
  values.push_back(drills::normalize_degree_index(top_degree + 1));
  values.push_back(drills::normalize_degree_index(top_degree - 1));
  std::sort(values.begin(), values.end());
  values.erase(std::unique(values.begin(), values.end()), values.end());
  return values;
}

// Core chord materials shared across different playback styles.
struct ChordQuestionCore {
  int root_degree = 0;
  bool add_seventh = false;
  std::string quality;
  ChordVoicingEngine::TriadQuality quality_enum = ChordVoicingEngine::TriadQuality::Major;
  const ChordVoicingEngine::RightHandPattern* right_pattern = nullptr;
  const ChordVoicingEngine::BassPattern* bass_pattern = nullptr;
  std::string right_voicing_id;
  std::string bass_voicing_id;
  int bass_offset = 0;
  int bass_degree = 0;
  std::vector<int> right_degrees;
  int tonic_midi = 60;
  std::string profile_id;
  int top_degree = 0;
};

ChordQuestionCore prepare_chord_question_core(const DrillSpec& spec,
                                              std::uint64_t& rng_state,
                                              std::optional<int>& last_degree,
                                              std::optional<std::string>& last_voicing,
                                              std::optional<int>& last_top_degree,
                                              const std::optional<std::string>& preferred_right,
                                              const std::optional<std::string>& preferred_bass,
                                              std::string_view profile_id) {
  ChordQuestionCore core;
  core.root_degree = pick_degree(spec, rng_state, last_degree);
  last_degree = core.root_degree;

  if (spec.params.contains("add_seventh")) {
    core.add_seventh = spec.params["add_seventh"].get<bool>();
  }

  core.quality = chord_quality_for_degree(core.root_degree);
  core.quality_enum = triad_quality_from_string(core.quality);
  const auto& voicing_eng = ChordVoicingEngine::instance();
  core.profile_id = std::string(voicing_eng.resolve_profile_id(profile_id));

  std::optional<std::string> avoid_voicing;
  if (avoid_repetition(spec) && last_voicing.has_value()) {
    avoid_voicing = last_voicing;
  }

  std::optional<std::string> desired_right = preferred_right;

  const auto& right_options = voicing_eng.right_hand_options(core.quality_enum, core.profile_id);

  if (!desired_right.has_value()) {
    std::vector<const ChordVoicingEngine::RightHandPattern*> candidates;
    candidates.reserve(right_options.size());
    for (const auto& option : right_options) {
      candidates.push_back(&option);
    }

    auto apply_filter = [&](const std::vector<int>& allowed) {
      if (allowed.empty()) {
        return;
      }
      std::vector<const ChordVoicingEngine::RightHandPattern*> filtered;
      filtered.reserve(candidates.size());
      for (const auto* candidate : candidates) {
        int top_deg = pattern_top_degree(*candidate, core.root_degree);
        if (std::find(allowed.begin(), allowed.end(), top_deg) != allowed.end()) {
          filtered.push_back(candidate);
        }
      }
      if (!filtered.empty()) {
        candidates = std::move(filtered);
      }
    };

    auto allowed_top = spec_param_degree_list(spec, "chord_allowed_top_degrees");
    if (!allowed_top.empty()) {
      apply_filter(allowed_top);
    }

    bool smooth = spec_param_bool(spec, "chord_voice_leading_continuity", false);
    if (smooth && last_top_degree.has_value()) {
      auto before = candidates;
      apply_filter(continuity_degrees(*last_top_degree));
      if (candidates.empty()) {
        candidates = std::move(before);
      }
    }

    if (avoid_voicing.has_value() && candidates.size() > 1) {
      candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
                                      [&](const auto* candidate) {
                                        return candidate->id == avoid_voicing.value();
                                      }),
                       candidates.end());
      if (candidates.empty()) {
        candidates.reserve(right_options.size());
        for (const auto& option : right_options) {
          candidates.push_back(&option);
        }
      }
    }

    if (candidates.empty()) {
      candidates.reserve(right_options.size());
      for (const auto& option : right_options) {
        candidates.push_back(&option);
      }
    }

    std::size_t pick_idx = candidates.size() == 1
                               ? 0
                               : static_cast<std::size_t>(
                                     rand_int(rng_state, 0,
                                              static_cast<int>(candidates.size()) - 1));
    desired_right = candidates[pick_idx]->id;
  }

  auto selection = voicing_eng.pick_triad(core.quality_enum, rng_state,
                                          desired_right, preferred_bass, avoid_voicing,
                                          core.profile_id);
  if (!selection.right_hand || !selection.bass) {
    throw std::runtime_error("ChordDrill: missing voicing definition for quality '" + core.quality + "'");
  }

  core.right_pattern = selection.right_hand;
  core.bass_pattern = selection.bass;
  core.right_voicing_id = selection.right_hand->id;
  core.bass_voicing_id = selection.bass->id;
  core.bass_offset = selection.bass->degree_offset;
  core.bass_degree = core.root_degree + core.bass_offset;
  core.tonic_midi = drills::central_tonic_midi(spec.key);
  core.top_degree = pattern_top_degree(*core.right_pattern, core.root_degree);

  core.right_degrees.reserve(core.right_pattern->degree_offsets.size());
  for (int offset : core.right_pattern->degree_offsets) {
    core.right_degrees.push_back(core.root_degree + offset);
  }

  last_voicing = core.right_voicing_id;
  last_top_degree = core.top_degree;
  return core;
}

int select_bass_midi(const ChordQuestionCore& core) {
  int bass_base = core.tonic_midi + drills::degree_to_offset(core.bass_degree);
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
  return drills::clamp_to_range(best_bass, 0, 127);
}

std::vector<int> voice_right_hand_midi(const ChordQuestionCore& core, int bass_midi) {
  std::vector<int> base_right_midi;
  base_right_midi.reserve(core.right_degrees.size());
  for (int degree_value : core.right_degrees) {
    base_right_midi.push_back(core.tonic_midi + drills::degree_to_offset(degree_value));
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
  for (int midi : chosen_right) {
    while (midi <= previous) {
      midi += 12;
    }
    midi = drills::clamp_to_range(midi, 0, 127);
    right_midi.push_back(midi);
    previous = midi;
  }
  return right_midi;
}

int find_voicing_index(const std::vector<ChordVoicingEngine::RightHandPattern>& options,
                       const std::string& id) {
  for (std::size_t i = 0; i < options.size(); ++i) {
    if (options[i].id == id) {
      return static_cast<int>(i);
    }
  }
  return 0;
}

} // namespace

void ChordDrill::configure(const DrillSpec& spec) {
  spec_ = spec;
  last_degree_.reset();
  last_voicing_id_.reset();
  last_top_degree_.reset();
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
  ensure_params_object(adjusted.params);

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
  auto core = prepare_chord_question_core(spec_, rng_state, last_degree_, last_voicing_id_,
                                          last_top_degree_,
                                          preferred_right_voicing_, preferred_bass_voicing_,
                                          voicing_source_id_);
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
  int sampled_voicing_index = find_voicing_index(right_options, sampled_voicing_id);

  PromptPlan plan;
  plan.modality = "midi-clip";
  plan.tempo_bpm = spec_.tempo_bpm;
  plan.count_in = false;

  const int prompt_tempo = plan.tempo_bpm.has_value() ? plan.tempo_bpm.value() : 90;
  int chord_dur_ms = spec_param_int(spec_, "chord_prompt_duration_ms", 900);
  if (chord_dur_ms <= 0) {
    chord_dur_ms = 900;
  }
  const int default_program = spec_param_int(spec_, "chord_prompt_program", 0);
  const bool split_tracks = spec_param_bool(spec_, "chord_prompt_split_tracks", true);
  const int strum_step_ms = spec_param_int(spec_, "chord_prompt_strum_step_ms", 0);
  const int default_velocity = spec_param_int(spec_, "chord_prompt_velocity", 90);

  int tonic_midi = core.tonic_midi;
  int bass_midi = select_bass_midi(core);

  nlohmann::json midi_tones = nlohmann::json::array();
  nlohmann::json realised_degrees = nlohmann::json::array();
  midi_tones.push_back(bass_midi);
  realised_degrees.push_back(bass_degree);
  plan.notes.push_back({bass_midi, chord_dur_ms, std::nullopt, std::nullopt});

  std::vector<int> right_midi = voice_right_hand_midi(core, bass_midi);
  for (std::size_t i = 0; i < right_midi.size(); ++i) {
    realised_degrees.push_back(right_degrees[i]);
    midi_tones.push_back(right_midi[i]);
    plan.notes.push_back({right_midi[i], chord_dur_ms, std::nullopt, std::nullopt});
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
    const int bass_channel = spec_param_int(spec_, "chord_prompt_bass_channel", 1);
    const int bass_program = spec_param_int(spec_, "chord_prompt_bass_program", default_program);
    auto bass_track = make_track("bass", bass_channel, bass_program);
    bass_track["notes"].push_back(make_note(bass_midi, 0, chord_dur_ms, default_velocity));

    const int right_channel = spec_param_int(spec_, "chord_prompt_right_channel", 0);
    const int right_program = spec_param_int(spec_, "chord_prompt_right_program", default_program);
    auto right_track = make_track("right", right_channel, right_program);
    for (std::size_t i = 0; i < right_midi.size(); ++i) {
      int onset_ms = strum_step_ms > 0 ? static_cast<int>(i) * strum_step_ms : 0;
      right_track["notes"].push_back(make_note(right_midi[i], onset_ms, chord_dur_ms, default_velocity));
    }

    tracks.push_back(std::move(bass_track));
    tracks.push_back(std::move(right_track));
  } else {
    const int merged_channel = spec_param_int(spec_, "chord_prompt_channel", 0);
    auto prompt_track = make_track("prompt", merged_channel, default_program);
    prompt_track["notes"].push_back(make_note(bass_midi, 0, chord_dur_ms, default_velocity));
    for (std::size_t i = 0; i < right_midi.size(); ++i) {
      int onset_ms = strum_step_ms > 0 ? static_cast<int>(i) * strum_step_ms : 0;
      prompt_track["notes"].push_back(make_note(right_midi[i], onset_ms, chord_dur_ms, default_velocity));
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
  question_payload["voicings_source"] = core.profile_id;

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
  last_top_degree_ = core.top_degree;
  return bundle;
}

} // namespace ear
