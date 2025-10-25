#include "chord_core.hpp"

#include "common.hpp"

#include "../src/rng.hpp"

#include <algorithm>
#include <limits>
#include <unordered_map>

namespace ear::drills::chord {
namespace {

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

int pattern_top_degree(const ear::ChordVoicingEngine::RightHandPattern& pattern,
                       int root_degree) {
  if (pattern.degree_offsets.empty()) {
    return drills::normalize_degree_index(root_degree);
  }
  int top_offset =
      *std::max_element(pattern.degree_offsets.begin(), pattern.degree_offsets.end());
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

std::vector<int> normalized_top_allowance(const ChordParams& params) {
  auto raw = params.allowed_top_degrees;
  for (int& value : raw) {
    value = drills::normalize_degree_index(value);
  }
  std::sort(raw.begin(), raw.end());
  raw.erase(std::unique(raw.begin(), raw.end()), raw.end());
  return raw;
}

} // namespace

ChordQuestionCore prepare_chord_question(const ChordParams& params,
                                          const std::string key,
                                         int root_degree,
                                         std::uint64_t& rng_state,
                                         ChordSelectionState& selection_state,
                                         const std::optional<std::string>& preferred_right,
                                         const std::optional<std::string>& preferred_bass,
                                         std::string_view profile_id) {
  ChordQuestionCore core;
  core.root_degree = root_degree;
  selection_state.last_degree = core.root_degree;
  
  core.add_seventh = params.add_seventh;

  core.quality = chord_quality_for_degree(core.root_degree);
  core.quality_enum = triad_quality_from_string(core.quality);
  const auto& voicing_eng = ChordVoicingEngine::instance();
  core.profile_id = std::string(voicing_eng.resolve_profile_id(profile_id));

  std::optional<std::string> avoid_voicing = selection_state.last_voicing_id;

  std::optional<std::string> chosen_right = preferred_right;

  const auto& right_options = voicing_eng.right_hand_options(core.quality_enum, core.profile_id);

  if (!chosen_right.has_value()) {
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

    auto allowed_top = normalized_top_allowance(params);
    if (!allowed_top.empty()) {
      apply_filter(allowed_top);
    }

    if (params.voice_leading_continuity && selection_state.last_top_degree.has_value()) {
      auto snapshot = candidates;
      apply_filter(continuity_degrees(selection_state.last_top_degree.value()));
      if (candidates.empty()) {
        candidates = std::move(snapshot);
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
    chosen_right = candidates[pick_idx]->id;
  }

  auto selection = voicing_eng.pick_triad(core.quality_enum, rng_state, chosen_right,
                                          preferred_bass, avoid_voicing, core.profile_id);
  if (!selection.right_hand || !selection.bass) {
    throw std::runtime_error("ChordDrill: missing voicing definition for quality '" +
                             core.quality + "'");
  }

  core.right_pattern = selection.right_hand;
  core.bass_pattern = selection.bass;
  core.right_voicing_id = selection.right_hand->id;
  core.bass_voicing_id = selection.bass->id;
  core.bass_offset = selection.bass->degree_offset;
  core.bass_degree = core.root_degree + core.bass_offset;
  core.tonic_midi = drills::central_tonic_midi(key);
  core.top_degree = pattern_top_degree(*core.right_pattern, core.root_degree);

  core.right_degrees.reserve(core.right_pattern->degree_offsets.size());
  for (int offset : core.right_pattern->degree_offsets) {
    core.right_degrees.push_back(core.root_degree + offset);
  }

  selection_state.last_voicing_id = core.right_voicing_id;
  selection_state.last_top_degree = core.top_degree;
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

std::vector<int> voice_right_hand_midi(const ChordParams& params,
                                       const ChordQuestionCore& core,
                                       int bass_midi,
                                       const std::optional<int>& previous_top_midi) {
  std::vector<int> base_right_midi;
  base_right_midi.reserve(core.right_degrees.size());
  for (int degree_value : core.right_degrees) {
    base_right_midi.push_back(core.tonic_midi + drills::degree_to_offset(degree_value));
  }

  constexpr int kRightTarget = 60; // C4
  const bool continuity = params.voice_leading_continuity;

  std::vector<int> best_voicing = base_right_midi;
  auto evaluate_candidate = [&](const std::vector<int>& candidate) {
    if (continuity && previous_top_midi.has_value() && !candidate.empty()) {
      double top_gap = std::abs(candidate.back() - previous_top_midi.value());
      double center_penalty =
          std::abs(center_of_mass(candidate) - static_cast<double>(kRightTarget)) * 0.1;
      return top_gap + center_penalty;
    }
    return std::abs(center_of_mass(candidate) - static_cast<double>(kRightTarget));
  };

  double best_score = std::numeric_limits<double>::infinity();
  std::vector<int> candidate;
  candidate.reserve(base_right_midi.size());

  for (int global_shift = -2; global_shift <= 2; ++global_shift) {
    candidate = base_right_midi;
    for (int& value : candidate) {
      value += 12 * global_shift;
    }

    std::vector<int> realised;
    realised.reserve(candidate.size());
    int previous = bass_midi;
    bool valid = true;
    for (int midi : candidate) {
      while (midi <= previous) {
        midi += 12;
        if (midi > 127) {
          valid = false;
          break;
        }
      }
      if (!valid) {
        break;
      }
      midi = drills::clamp_to_range(midi, 0, 127);
      realised.push_back(midi);
      previous = midi;
    }
    if (!valid || realised.size() != candidate.size()) {
      continue;
    }

    double score = evaluate_candidate(realised);
    if (score < best_score) {
      best_score = score;
      best_voicing = std::move(realised);
    }
  }

  if (best_score < std::numeric_limits<double>::infinity()) {
    return best_voicing;
  }

  std::vector<int> fallback;
  fallback.reserve(base_right_midi.size());
  int previous = bass_midi;
  for (int midi : base_right_midi) {
    while (midi <= previous) {
      midi += 12;
    }
    midi = drills::clamp_to_range(midi, 0, 127);
    fallback.push_back(midi);
    previous = midi;
  }
  return fallback;
}

int find_voicing_index(const std::vector<ear::ChordVoicingEngine::RightHandPattern>& options,
                       const std::string& id) {
  for (std::size_t i = 0; i < options.size(); ++i) {
    if (options[i].id == id) {
      return static_cast<int>(i);
    }
  }
  return 0;
}

// ChordParams::TrainingRootConfig resolve_training_root_config(const ChordParams& params, DrillSpec& spec,
//                                                 bool split_tracks,
//                                                 int chord_duration_ms,
//                                                 int default_velocity,
//                                                 int right_channel_fallback,
//                                                 int merged_channel_fallback) {
//   ChordParams::TrainingRootConfig config;
//   config.enabled = param_flag(spec, "training_root_enabled", false);
//   config.delay_beats = param_double(spec, "training_root_delay_beats", 0.0);
//   config.duration_ms = param_int(spec, "training_root_duration_ms", chord_duration_ms);
//   config.enabled = params.training_root.value().enabled;
//   if (config.duration_ms <= 0) {
//     config.duration_ms = chord_duration_ms;
//   }
//   config.channel = param_int(spec, "training_root_channel",
//                              split_tracks ? right_channel_fallback : merged_channel_fallback);
//   config.program = param_int(spec, "training_root_program", 0);
//   config.velocity = param_int(spec, "training_root_velocity", default_velocity);
//   return config;
// }

int adjust_helper_midi(int helper_midi, int bass_midi, const std::vector<int>& right_midi) {
  if (right_midi.empty()) {
    return helper_midi;
  }
  int lowest = std::min(bass_midi, right_midi.front());
  int highest = right_midi.back();
  while (helper_midi < lowest - 12) {
    helper_midi += 12;
  }
  while (helper_midi > highest + 12) {
    helper_midi -= 12;
  }
  return helper_midi;
}

} // namespace ear::drills::chord
