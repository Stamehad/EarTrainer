#pragma once

#include "../include/ear/drill_spec.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ear::drills {

constexpr int kCentralTonicLow = 53;  // F3
constexpr int kCentralTonicHigh = 64; // E4

inline int normalize_degree_index(int degree) {
  int idx = degree % 7;
  if (idx < 0) {
    idx += 7;
  }
  return idx;
}

inline int degree_to_offset(int degree) {
  static const int scale_steps[7] = {0, 2, 4, 5, 7, 9, 11};
  int idx = normalize_degree_index(degree);
  int octave = (degree - idx) / 7;
  return octave * 12 + scale_steps[idx];
}

inline int tonic_from_key(const std::string& key) {
  static const std::unordered_map<std::string, int> offsets = {
      {"C", 0},  {"C#", 1}, {"Db", 1}, {"D", 2},  {"D#", 3}, {"Eb", 3},
      {"E", 4},  {"F", 5},  {"F#", 6}, {"Gb", 6}, {"G", 7},  {"G#", 8},
      {"Ab", 8}, {"A", 9},  {"A#", 10},{"Bb", 10},{"B", 11}};

  if (key.empty()) {
    return 60; // default middle C
  }

  auto space_pos = key.find(' ');
  std::string tonic = space_pos == std::string::npos ? key : key.substr(0, space_pos);

  if (tonic.empty()) {
    return 60;
  }

  tonic[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(tonic[0])));
  if (tonic.size() >= 2 && (tonic[1] == '#' || tonic[1] == 'b')) {
    tonic = tonic.substr(0, 2);
  } else {
    tonic = tonic.substr(0, 1);
  }

  auto it = offsets.find(tonic);
  int base = it == offsets.end() ? 0 : it->second;
  return 60 + base;
}

inline int central_tonic_midi(const std::string& key) {
  int tonic = tonic_from_key(key);
  while (tonic < kCentralTonicLow) {
    tonic += 12;
  }
  while (tonic > kCentralTonicHigh) {
    tonic -= 12;
  }
  return tonic;
}

inline int clamp_to_range(int midi, int min, int max) {
  return std::max(min, std::min(max, midi));
}

inline std::pair<int, int> semitone_window(const DrillSpec& spec, int default_span) {
  int below = default_span;
  int above = default_span;
  if (spec.params.is_object()) {
    if (spec.params.contains("range_below_semitones")) {
      below = std::max(0, spec.params["range_below_semitones"].get<int>());
    }
    if (spec.params.contains("range_above_semitones")) {
      above = std::max(0, spec.params["range_above_semitones"].get<int>());
    }
  }
  return {below, above};
}

inline std::pair<int, int> relative_bounds(const DrillSpec& spec, int default_span) {
  int tonic = central_tonic_midi(spec.key);
  auto window = semitone_window(spec, default_span);
  const int below = window.first;
  const int above = window.second;
  int lower = std::max(0, tonic - below);
  int upper = std::min(127, tonic + above);
  if (lower > upper) {
    std::swap(lower, upper);
  }
  return {lower, upper};
}

inline std::vector<int> midi_candidates_for_degree(const DrillSpec& spec, int degree,
                                                   int semitone_span) {
  auto bounds = relative_bounds(spec, semitone_span);
  const int lower = bounds.first;
  const int upper = bounds.second;

  int tonic = central_tonic_midi(spec.key);
  int midi = tonic + degree_to_offset(degree);
  while (midi > upper) {
    midi -= 12;
  }
  while (midi < lower) {
    midi += 12;
  }

  std::vector<int> candidates;
  for (int candidate = midi; candidate >= lower; candidate -= 12) {
    candidates.push_back(candidate);
  }
  for (int candidate = midi + 12; candidate <= upper; candidate += 12) {
    candidates.push_back(candidate);
  }

  std::sort(candidates.begin(), candidates.end());
  candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
  return candidates;
}

inline int degree_to_midi(const DrillSpec& spec, int degree) {
  int span = 12;
  if (spec.params.is_object()) {
    if (spec.params.contains("range_below_semitones") || spec.params.contains("range_above_semitones")) {
      int below = spec.params.contains("range_below_semitones") ? spec.params["range_below_semitones"].get<int>() : 12;
      int above = spec.params.contains("range_above_semitones") ? spec.params["range_above_semitones"].get<int>() : 12;
      span = std::max({1, below, above});
    } else if (spec.params.contains("note_range_semitones")) {
      span = std::max(1, spec.params["note_range_semitones"].get<int>());
    } else if (spec.params.contains("chord_range_semitones")) {
      span = std::max(1, spec.params["chord_range_semitones"].get<int>());
    }
  }

  auto candidates = midi_candidates_for_degree(spec, degree, span);
  if (!candidates.empty()) {
    int base = central_tonic_midi(spec.key) + degree_to_offset(degree);
    auto it = std::min_element(candidates.begin(), candidates.end(),
                               [&](int lhs, int rhs) {
                                 return std::abs(lhs - base) < std::abs(rhs - base);
                               });
    return *it;
  }

  int tonic = central_tonic_midi(spec.key);
  int midi = tonic + degree_to_offset(degree);
  midi = clamp_to_range(midi, 0, 127);
  return midi;
}

} // namespace ear::drills
