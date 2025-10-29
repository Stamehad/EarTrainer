#include "harmony.hpp"

#include "common.hpp"
#include "../include/ear/question_bundle_v2.hpp"
#include "../include/ear/midi_clip.hpp"
#include "../src/rng.hpp"

#include <algorithm>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace ear {
namespace {

using Pattern = HarmonyClusterPattern;

std::vector<int> base_degrees() {
  return {0, 1, 2, 3, 4, 5, 6};
}

const std::unordered_map<int, std::vector<std::vector<int>>>& cluster_catalog() {
  static const std::unordered_map<int, std::vector<std::vector<int>>> kCatalog = {
      {1, {{0, 2, 4}, {0, 2, 5}, {0, 1, 4}, {0, 3, 4}, {0, 3, 5}}},
      {2, {{0, 2, 7}, {0, 3, 7}, {0, 4, 7}, {0, 5, 7}}},
      {3, {{0, 2, 6}, {0, 1, 3}, {0, 4, 6}, {0, 1, 5}, {0, 2, 3}, {0, 4, 5}}},
      {4, {{0, 3, 6}, {0, 4, 8}}},
      {5, {{0, 4, 9}, {0, 5, 9}, {0, 5, 10}}},
      {6, {{0, 3, 8}, {0, 3, 9}, {0, 5, 8}, {0, 6, 9}}},
  };
  return kCatalog;
}

std::vector<int> default_cluster_ids() {
  return {1, 2, 3, 4, 5, 6};
}

Pattern make_pattern(int cluster_id,
                     std::size_t idx,
                     const std::vector<int>& offsets) {
  Pattern pattern;
  pattern.cluster_id = cluster_id;
  pattern.id = "cluster" + std::to_string(cluster_id) + "_" + std::to_string(idx);
  pattern.relative_degrees = offsets;
  return pattern;
}

std::vector<Pattern> build_patterns(const std::vector<int>& cluster_ids) {
  std::vector<Pattern> patterns;
  const auto& catalog = cluster_catalog();
  for (int id : cluster_ids) {
    auto it = catalog.find(id);
    if (it == catalog.end()) {
      continue;
    }
    const auto& entries = it->second;
    for (std::size_t idx = 0; idx < entries.size(); ++idx) {
      const auto& offsets = entries[idx];
      if (offsets.size() != 3) {
        continue;
      }
      patterns.push_back(make_pattern(id, idx, offsets));
    }
  }
  if (patterns.empty()) {
    // Fallback to default catalog if spec filtered everything out.
    for (int id : default_cluster_ids()) {
      auto it = catalog.find(id);
      if (it == catalog.end()) {
        continue;
      }
      const auto& entries = it->second;
      for (std::size_t idx = 0; idx < entries.size(); ++idx) {
        const auto& offsets = entries[idx];
        if (offsets.size() != 3) {
          continue;
        }
        patterns.push_back(make_pattern(id, idx, offsets));
      }
    }
  }
  return patterns;
}

} // namespace

//====================================================================
// INITIALIZE HARMONY DRILL
//====================================================================
void HarmonyDrill::configure(const DrillSpec& spec) {
  spec_ = spec;
  params = std::get<IntervalParams>(spec_.params);
  tonic_midi_ = drills::central_tonic_midi(spec_.key);
  avoid_repeat_ = params.avoid_repeat;
  allowed_root_degrees_ = params.allowed_degrees;
  patterns_ = build_patterns(params.cluster_ids);
  last_root_degree_.reset();
  last_pattern_index_.reset();
}

//====================================================================
// NEXT QUESTION -> QUESTION BUNDLE
//====================================================================
QuestionBundle HarmonyDrill::next_question(std::uint64_t& rng_state) {
  if (patterns_.empty()) {
    throw std::runtime_error("Harmony drill has no available clusters.");
  }

  //-----------------------------------------------------------------
  // SAMPLE HARMONY PATTERN
  //-----------------------------------------------------------------
  // SAMPLE ROOT DEGREE
  std::vector<int> degree_candidates = allowed_root_degrees_;
  if (avoid_repeat_ && last_root_degree_ && degree_candidates.size() > 1) {
    degree_candidates.erase(
        std::remove(degree_candidates.begin(), degree_candidates.end(), *last_root_degree_),
        degree_candidates.end());
    if (degree_candidates.empty()) {
      degree_candidates = allowed_root_degrees_;
    }
  }
  int degree_idx = rand_int(rng_state, 0, static_cast<int>(degree_candidates.size()) - 1);
  int root_degree = degree_candidates[static_cast<std::size_t>(degree_idx)];

  // SAMPLE PATTERN
  std::vector<std::size_t> pattern_indices(patterns_.size());
  std::iota(pattern_indices.begin(), pattern_indices.end(), std::size_t{0});
  if (avoid_repeat_ && last_pattern_index_ && pattern_indices.size() > 1) {
    pattern_indices.erase(std::remove(pattern_indices.begin(), pattern_indices.end(),
                                      *last_pattern_index_),
                          pattern_indices.end());
    if (pattern_indices.empty()) {
      pattern_indices.resize(patterns_.size());
      std::iota(pattern_indices.begin(), pattern_indices.end(), std::size_t{0});
    }
  }
  int pattern_choice =
      rand_int(rng_state, 0, static_cast<int>(pattern_indices.size()) - 1);
  std::size_t pattern_index = pattern_indices[static_cast<std::size_t>(pattern_choice)];
  const auto& pattern = patterns_.at(pattern_index);

  // COMPUTE DEGREES + MIDIS
  std::vector<int> degrees;
  degrees.reserve(pattern.relative_degrees.size());
  for (int offset : pattern.relative_degrees) {
    degrees.push_back(root_degree + offset);
  }

  std::vector<int> midis;
  midis.reserve(degrees.size());
  for (int degree : degrees) {
    int midi = tonic_midi_ + drills::degree_to_offset(degree);
    midis.push_back(midi);
  }

  //-----------------------------------------------------
  // PREPARE QUESTION AND ANSWER
  //-----------------------------------------------------
  HarmonyAnswerV2 answer{degrees};
  HarmonyQuestionV2 question{tonic_midi_, spec_.key, spec_.quality,
                             static_cast<int>(degrees.size()), degrees, std::nullopt};

  //-----------------------------------------------------------------
  // GENERATE MIDI-CLIP
  //-----------------------------------------------------------------
  MidiClipBuilder builder(params.tempo_bpm, 480);
  auto track = builder.add_track("harmony", 0, params.program);
  Beats start{0.0};
  builder.add_chord(track, start, Beats{params.note_beat}, midis, params.velocity);

  if (params.add_helper) {
    // ADD HELPER MELODY (ROOTS)
    auto helper_track = builder.add_track("helper", 1, 0); // PIANO
    Beats helper_start{1.0};
    for (int midi : midis) {
      builder.add_note(helper_track, helper_start, Beats{params.note_beat}, midi, 64);
      helper_start.advance_by(0.5);
    }
  }

  //-----------------------------------------------------------------
  // GENERATE QUESTION BUNDLE
  //-----------------------------------------------------------------
  QuestionBundle bundle;
  bundle.question_id.clear();
  bundle.question = question;
  bundle.correct_answer = answer;
  bundle.prompt_clip = builder.build();

  last_root_degree_ = root_degree;
  last_pattern_index_ = pattern_index;

  return bundle;
}

} // namespace ear
