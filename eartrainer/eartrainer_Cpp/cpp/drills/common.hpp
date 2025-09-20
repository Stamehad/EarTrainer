#pragma once

#include "ear/types.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>

namespace ear::drills {

inline int degree_to_offset(int degree) {
  static const int scale_steps[7] = {0, 2, 4, 5, 7, 9, 11};
  int octave = (degree - 1) / 7;
  int idx = (degree - 1) % 7;
  if (idx < 0) {
    idx += 7;
    octave -= 1;
  }
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

inline int clamp_to_range(int midi, int min, int max) {
  return std::max(min, std::min(max, midi));
}

inline int degree_to_midi(const SessionSpec& spec, int degree) {
  int tonic = tonic_from_key(spec.key);
  int midi = tonic + degree_to_offset(degree);
  while (midi < spec.range_min) {
    midi += 12;
  }
  while (midi > spec.range_max) {
    midi -= 12;
  }
  return clamp_to_range(midi, spec.range_min, spec.range_max);
}

} // namespace ear::drills

