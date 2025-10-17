#include "../include/ear/chord_voicings.hpp"

#include "../src/rng.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <stdexcept>

namespace ear {
namespace {

using Quality = ChordVoicingEngine::TriadQuality;
using BassPattern = ChordVoicingEngine::BassPattern;
using RightPattern = ChordVoicingEngine::RightHandPattern;

std::vector<BassPattern> make_triad_bass_patterns() {
  return {
      {"root_low", -14},
      {"root", 0},
      {"first_inv", 2},
      {"second_inv", 4},
  };
}

std::vector<RightPattern> make_triad_right_patterns() {
  return {
      {"root_pos", {0, 2, 4}},
      {"first_inv", {2, 4, 7}},
      {"second_inv", {4, 7, 9}},
      {"root_with_octave", {0, 2, 4, 7}},
      {"drop2_cluster", {-3, 0, 2}},
  };
}

std::size_t index_for(Quality quality) {
  return static_cast<std::size_t>(quality);
}

const char* quality_name(Quality quality) {
  switch (quality) {
  case Quality::Major:
    return "major";
  case Quality::Minor:
    return "minor";
  case Quality::Diminished:
    return "diminished";
  }
  return "major";
}

} // namespace

ChordVoicingEngine::ChordVoicingEngine() {
  const auto bass = make_triad_bass_patterns();
  const auto right = make_triad_right_patterns();

  triads_[index_for(Quality::Major)].bass = bass;
  triads_[index_for(Quality::Major)].right = right;

  triads_[index_for(Quality::Minor)].bass = bass;
  triads_[index_for(Quality::Minor)].right = right;

  triads_[index_for(Quality::Diminished)].bass = bass;
  triads_[index_for(Quality::Diminished)].right = right;
}

const ChordVoicingEngine& ChordVoicingEngine::instance() {
  static ChordVoicingEngine engine;
  return engine;
}

std::size_t ChordVoicingEngine::quality_index(TriadQuality quality) {
  return index_for(quality);
}

const std::vector<ChordVoicingEngine::BassPattern>&
ChordVoicingEngine::bass_options(TriadQuality quality) const {
  return triads_[quality_index(quality)].bass;
}

const std::vector<ChordVoicingEngine::RightHandPattern>&
ChordVoicingEngine::right_hand_options(TriadQuality quality) const {
  return triads_[quality_index(quality)].right;
}

const ChordVoicingEngine::BassPattern&
ChordVoicingEngine::bass(TriadQuality quality, const std::string& id) const {
  const auto& options = bass_options(quality);
  auto it = std::find_if(options.begin(), options.end(),
                         [&id](const BassPattern& pattern) { return pattern.id == id; });
  if (it == options.end()) {
    throw std::out_of_range("Unknown bass voicing id '" + id + "'");
  }
  return *it;
}

const ChordVoicingEngine::RightHandPattern&
ChordVoicingEngine::right_hand(TriadQuality quality, const std::string& id) const {
  const auto& options = right_hand_options(quality);
  auto it = std::find_if(options.begin(), options.end(),
                         [&id](const RightPattern& pattern) { return pattern.id == id; });
  if (it == options.end()) {
    throw std::out_of_range("Unknown right-hand voicing id '" + id + "'");
  }
  return *it;
}

ChordVoicingEngine::Selection
ChordVoicingEngine::pick_triad(TriadQuality quality,
                               std::uint64_t& rng_state,
                               std::optional<std::string> preferred_right,
                               std::optional<std::string> preferred_bass,
                               std::optional<std::string> avoid_right) const {
  const auto& basses = bass_options(quality);
  const auto& rights = right_hand_options(quality);

  if (basses.empty() || rights.empty()) {
    throw std::runtime_error("ChordVoicingEngine: triad voicings unavailable for quality '" +
                             std::string(quality_name(quality)) + "'");
  }

  const BassPattern* bass_choice = nullptr;
  const RightPattern* right_choice = nullptr;

  if (preferred_bass.has_value()) {
    bass_choice = &bass(quality, *preferred_bass);
  } else {
    bass_choice = &basses.front();
  }

  if (preferred_right.has_value()) {
    right_choice = &right_hand(quality, *preferred_right);
  } else {
    std::size_t count = rights.size();
    std::size_t idx = 0;
    if (avoid_right.has_value() && count > 1) {
      std::vector<std::size_t> candidates;
      candidates.reserve(count);
      for (std::size_t i = 0; i < count; ++i) {
        if (rights[i].id != avoid_right.value()) {
          candidates.push_back(i);
        }
      }
      if (candidates.empty()) {
        idx = 0;
      } else {
        std::size_t pick = static_cast<std::size_t>(
            rand_int(rng_state, 0, static_cast<int>(candidates.size()) - 1));
        idx = candidates[pick];
      }
    } else {
      idx = static_cast<std::size_t>(
          rand_int(rng_state, 0, static_cast<int>(count) - 1));
    }
    right_choice = &rights[idx];
  }

  Selection selection;
  selection.bass = bass_choice;
  selection.right_hand = right_choice;
  return selection;
}

ChordVoicingEngine::TriadQuality triad_quality_from_string(const std::string& quality) {
  std::string lower = quality;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  if (lower == "minor") {
    return Quality::Minor;
  }
  if (lower == "diminished" || lower == "dim") {
    return Quality::Diminished;
  }
  return Quality::Major;
}

std::string to_string(ChordVoicingEngine::TriadQuality quality) {
  return std::string(quality_name(quality));
}

} // namespace ear
