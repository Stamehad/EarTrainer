#include "chord.hpp"

#include "common.hpp"
#include "../src/rng.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <unordered_map>
#include <vector>

namespace ear {

namespace {

const std::unordered_map<std::string, std::vector<int>> kBassOffsets = {
    {"major", {-14}},
    {"minor", {-14}},
    {"diminished", {-14}},
};

const std::unordered_map<std::string, std::vector<std::vector<int>>> kRightHandVoicings = {
    {"major", {{0, 2, 4, 7}, {0, 2, 4}, {-3, 0, 2}}},
    {"minor", {{0, 2, 4, 7}, {0, 2, 4}, {-3, 0, 2}}},
    {"diminished", {{0, 2, 4, 7}, {0, 2, 4}, {-3, 0, 2}}},
};

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

const std::vector<int>& bass_offsets_for_quality(const std::string& quality) {
  auto it = kBassOffsets.find(quality);
  if (it != kBassOffsets.end()) {
    return it->second;
  }
  return kBassOffsets.at("major");
}

const std::vector<std::vector<int>>& right_hand_voicings_for_quality(const std::string& quality) {
  auto it = kRightHandVoicings.find(quality);
  if (it != kRightHandVoicings.end()) {
    return it->second;
  }
  return kRightHandVoicings.at("major");
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

int pick_voicing_index(const std::string& quality, std::uint64_t& rng_state) {
  const auto& voicings = right_hand_voicings_for_quality(quality);
  if (voicings.empty()) {
    return 0;
  }
  return rand_int(rng_state, 0, static_cast<int>(voicings.size()) - 1);
}

nlohmann::json build_degrees_payload(int root_degree, const std::string& quality,
                                     int voicing_index, int bass_offset,
                                     bool add_seventh = false) {
  const auto& right_voicings = right_hand_voicings_for_quality(quality);
  const auto& relative_offsets = right_voicings.empty()
                                     ? std::vector<int>{0, 2, 4, 7}
                                     : right_voicings[static_cast<std::size_t>(voicing_index) % right_voicings.size()];

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
  payload["voicing_index"] = voicing_index;
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

} // namespace

void ChordDrill::configure(const DrillSpec& spec) {
  spec_ = spec;
  last_degree_.reset();
  last_voicing_.reset();
}

DrillOutput ChordDrill::next_question(std::uint64_t& rng_state) {
  int degree = pick_degree(spec_, rng_state, last_degree_);
  last_degree_ = degree;

  bool add_seventh = false;
  if (spec_.params.contains("add_seventh")) {
    add_seventh = spec_.params["add_seventh"].get<bool>();
  }

  std::string quality = chord_quality_for_degree(degree);
  const auto& bass_options = bass_offsets_for_quality(quality);
  int bass_index = bass_options.empty() ? 0 : rand_int(rng_state, 0, static_cast<int>(bass_options.size()) - 1);
  int bass_offset = bass_options.empty() ? 0 : bass_options[static_cast<std::size_t>(bass_index)];

  int voicing_index = pick_voicing_index(quality, rng_state);
  if (avoid_repetition(spec_) && last_voicing_.has_value()) {
    const auto& voicings = right_hand_voicings_for_quality(quality);
    if (voicings.size() > 1) {
      for (int attempt = 0; attempt < 3 && voicing_index == last_voicing_.value(); ++attempt) {
        voicing_index = pick_voicing_index(quality, rng_state);
      }
    }
  }
  last_voicing_ = voicing_index;

  auto payload = build_degrees_payload(degree, quality, voicing_index, bass_offset, add_seventh);

  int root_degree = payload["root"].get<int>();
  std::string sampled_quality = payload["quality"].get<std::string>();
  int sampled_voicing_index = payload["voicing_index"].get<int>();
  int bass_degree = root_degree;
  if (payload.contains("bass_degree")) {
    bass_degree = payload["bass_degree"].get<int>();
  }
  int sampled_bass_offset = 0;
  if (payload.contains("bass_offset")) {
    sampled_bass_offset = payload["bass_offset"].get<int>();
  }

  nlohmann::json right_offsets_json = nlohmann::json::array();
  if (payload.contains("right_offsets")) {
    right_offsets_json = payload["right_offsets"];
  }
  nlohmann::json degrees_node = nlohmann::json::array();
  if (payload.contains("degrees")) {
    degrees_node = payload["degrees"];
  }
  auto right_degrees = to_vector(degrees_node);
  if (right_degrees.empty()) {
    auto right_offsets = to_vector(right_offsets_json);
    for (int offset : right_offsets) {
      right_degrees.push_back(root_degree + offset);
    }
  }

  PromptPlan plan;
  // Emit a block chord prompt: simultaneous note_on events encoded via midi-clip.
  // We indicate this intent by using a dedicated modality the JSON bridge recognizes.
  plan.modality = "midi_block";
  plan.tempo_bpm = spec_.tempo_bpm;
  plan.count_in = false;

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
  // Duration for the block chord in ms
  constexpr int kChordDurMs = 900;
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
    // For block chord prompt, all notes share the same onset; durations can be uniform.
    plan.notes.push_back({midi, kChordDurMs, std::nullopt, std::nullopt});
    previous = midi;
  }

  nlohmann::json question_payload = nlohmann::json::object();
  question_payload["root_degree"] = root_degree;
  question_payload["quality"] = sampled_quality;
  question_payload["degrees"] = realised_degrees;
  question_payload["voicing_midi"] = midi_tones;
  question_payload["tonic_midi"] = tonic_midi;
  question_payload["voicing_index"] = sampled_voicing_index;
  question_payload["bass_midi"] = bass_midi;
  nlohmann::json right_midi_json = nlohmann::json::array();
  for (int value : right_midi) {
    right_midi_json.push_back(value);
  }
  question_payload["right_hand_midi"] = right_midi_json;
  question_payload["bass_degree"] = bass_degree;
  question_payload["bass_offset"] = sampled_bass_offset;

  question_payload["right_offsets"] = right_offsets_json;

  nlohmann::json answer_payload = nlohmann::json::object();
  answer_payload["root_degree"] = root_degree;

  nlohmann::json hints = nlohmann::json::object();
  hints["answer_kind"] = "chord_degree";
  nlohmann::json allowed = nlohmann::json::array();
  allowed.push_back("Replay");
  allowed.push_back("GuideTone");
  hints["allowed_assists"] = allowed;
  nlohmann::json budget = nlohmann::json::object();
  for (const auto& entry : spec_.assistance_policy) {
    budget[entry.first] = entry.second;
  }
  hints["assist_budget"] = budget;

  return DrillOutput{TypedPayload{"chord", question_payload},
                     TypedPayload{"chord_degree", answer_payload},
                     plan,
                     hints};
}

} // namespace ear
