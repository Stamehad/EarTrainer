#include "../include/ear/chord_voicings.hpp"

#include "../src/rng.hpp"
#include "../include/ear/types.hpp"
#include "resources/drill_params.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <stdexcept>

namespace ear {
namespace {

using Quality = ChordVoicingEngine::TriadQuality;
using BassPattern = ChordVoicingEngine::BassPattern;
using RHChord = ChordVoicingEngine::RightHandPattern;
using RHChords = ChordVoicingEngine::RightHandPatterns;
using RightVoicing = ChordVoicingEngine::RightVoicing;
using BassChoice = ChordVoicingEngine::BassChoice;

constexpr std::array<Quality, 3> kAllQualities = {
    Quality::Major,
    Quality::Minor,
    Quality::Diminished,
};

std::vector<BassPattern> make_default_triad_bass_patterns() {
  return {
      {"root_low", -14},
      {"root", 0},
      {"first_inv", 2},
      {"second_inv", 4},
  };
}

std::vector<RHChord> make_default_triad_right_patterns() {
  return {
      {"root_pos", {0, 2, 4}},
      {"first_inv", {2, 4, 7}},
      {"second_inv", {4, 7, 9}},
      {"root_with_octave", {0, 2, 4, 7}},
      {"drop2_cluster", {-3, 0, 2}},
  };
}

std::vector<BassPattern> make_strings_triad_bass_patterns() {
  // Mirrors the bass offsets defined in resources/voicings/strings_voicing.json.
  return {
      {"strings_root_low", -14},
  };
}

std::vector<RHChord> make_strings_triad_right_patterns() {
  // Mirrors the degree offsets defined in resources/voicings/strings_voicing.json.
  return {
      {"strings_open_spread", {-7, -3, 2, 7}},
      {"strings_open_five_low", {-7, -3, 0, 4, 9}},
      {"strings_open_five_high", {-7, -3, 2, 7, 11}},
  };
}

std::vector<BassPattern> make_simple_triad_bass_patterns() {
  return {
      {"simple_root", 0},
  };
}

std::vector<RHChord> make_simple_triad_right_patterns() {
  return {
      {"simple_root", {0, 2, 4}},
  };
}

RHChords get_rh_chords(const DrillInstrument& inst){
  switch (inst){
    case DrillInstrument::Piano:
      return RHChords{std::move(make_default_triad_right_patterns())};
    case DrillInstrument::Strings:
      return RHChords{std::move(make_strings_triad_right_patterns())};
    default:
      return RHChords{std::move(make_default_triad_right_patterns())};
    //case DrillInstrument::Guitar;
  }
}

std::vector<BassPattern> get_bass_options(const DrillInstrument& inst){
  switch (inst){
    case DrillInstrument::Piano:
      return make_default_triad_bass_patterns();
    case DrillInstrument::Strings:
      return make_strings_triad_bass_patterns();
    default:
      return make_strings_triad_bass_patterns();
    //case DrillInstrument::Guitar;
  }
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

const std::string& ChordVoicingEngine::default_profile_id() {
  static const std::string kDefault = "builtin_diatonic_triads";
  return kDefault;
}

ChordVoicingEngine::ChordVoicingEngine() {
  Profile piano;
  piano.id = default_profile_id();
  const auto default_bass = make_default_triad_bass_patterns();
  const auto default_right = make_default_triad_right_patterns();
  for (Quality quality : kAllQualities) {
    auto& set = piano.triads[index_for(quality)];
    set.bass = default_bass;
    set.right = default_right;
  }
  profiles_.emplace(piano.id, std::move(piano));

  Profile strings;
  strings.id = "strings_ensemble";
  const auto strings_bass = make_strings_triad_bass_patterns();
  const auto strings_right = make_strings_triad_right_patterns();
  for (Quality quality : kAllQualities) {
    auto& set = strings.triads[index_for(quality)];
    set.bass = strings_bass;
    set.right = strings_right;
  }
  profiles_.emplace(strings.id, std::move(strings));

  Profile simple;
  simple.id = "simple_triads";
  const auto simple_bass = make_simple_triad_bass_patterns();
  const auto simple_right = make_simple_triad_right_patterns();
  for (Quality quality : kAllQualities) {
    auto& set = simple.triads[index_for(quality)];
    set.bass = simple_bass;
    set.right = simple_right;
  }
  profiles_.emplace(simple.id, std::move(simple));
}

ChordVoicingEngine& ChordVoicingEngine::instance() {
  static ChordVoicingEngine engine;
  return engine;
}

void ChordVoicingEngine::configure(
  const KeyType& keytype, 
  const DrillInstrument& inst, 
  int tonic_midi, 
  bool voice_leading_continuity
){
  keytype_ = keytype;                           // Major, Minor
  inst_ = inst;                                 // Piano, String
  tonic_midi_ = tonic_midi;
  continuity_ = voice_leading_continuity;
}

RightVoicing ChordVoicingEngine::get_voicing(int deg, std::uint64_t& rng_state){
  // GET VOICINGS -> SHIFT TO CHORD DEGREE -> SHIFT TO MIDIS
  RHChords rh_chords = get_rh_chords(inst_);
  RHChords shifted_chords = rh_chords.shift_to(deg);
  if (continuity_ && top_degree_.has_value()){
    RHChords new_chords = shifted_chords.filter_by_top_degree(top_degree_.value());
    shifted_chords = new_chords;
  }
  RHChords midi_chords = shifted_chords.to_midi(keytype_, tonic_midi_);

  // SAMPLE
  size_t size = midi_chords.size();
  int idx = rand_int(rng_state, 0, static_cast<int>(size) - 1);
  RHChord shifted_chord = shifted_chords[idx];
  RHChord midi_chord = midi_chords[idx];
  
  top_degree_ = shifted_chord.degree_offsets.back();
  int top_midi_ = midi_chord.degree_offsets.back();

  return RightVoicing{
    midi_chord.id,
    degree_to_quality(deg),
    shifted_chord.degree_offsets,
    midi_chord.degree_offsets,
    top_midi_
  };
}

BassChoice ChordVoicingEngine::get_bass(int deg, std::uint64_t& rng_state){
  // NOT SUPPORTING INVERSIONS YET!!
  BassPattern bass = make_default_triad_bass_patterns()[0];
  const int* scale_steps_ptr = get_scale_steps(keytype_);

  int bass_deg = bass.degree_offset + deg;
  int idx = bass_deg%7; if (idx < 0) idx+=7;
  int octave = (bass_deg-idx)/7;
  int bass_midi = scale_steps_ptr[static_cast<size_t>(idx)] + 12*octave + tonic_midi_;
  
  return BassChoice {
    bass.id, 
    0,                
    deg,              
    bass_midi             
  };
}

RHChords ChordVoicingEngine::get_ps(){
  return get_rh_chords(inst_);
}

std::size_t ChordVoicingEngine::quality_index(TriadQuality quality) {
  return index_for(quality);
}

const std::string& ChordVoicingEngine::resolve_profile_id(std::string_view profile_id) const {
  return profile_for(profile_id).id;
}

const std::vector<ChordVoicingEngine::BassPattern>&
ChordVoicingEngine::bass_options(TriadQuality quality, std::string_view profile_id) const {
  return profile_for(profile_id).triads[quality_index(quality)].bass;
}

const std::vector<ChordVoicingEngine::RightHandPattern>&
ChordVoicingEngine::right_hand_options(TriadQuality quality, std::string_view profile_id) const {
  return profile_for(profile_id).triads[quality_index(quality)].right;
}

const ChordVoicingEngine::BassPattern&
ChordVoicingEngine::bass(TriadQuality quality, const std::string& id,
                         std::string_view profile_id) const {
  const auto& options = bass_options(quality, profile_id);
  auto it = std::find_if(options.begin(), options.end(),
                         [&id](const BassPattern& pattern) { return pattern.id == id; });
  if (it == options.end()) {
    throw std::out_of_range("Unknown bass voicing id '" + id + "'");
  }
  return *it;
}

const ChordVoicingEngine::RightHandPattern&
ChordVoicingEngine::right_hand(TriadQuality quality, const std::string& id,
                               std::string_view profile_id) const {
  const auto& options = right_hand_options(quality, profile_id);
  auto it = std::find_if(options.begin(), options.end(),
                         [&id](const RHChord& pattern) { return pattern.id == id; });
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
                               std::optional<std::string> avoid_right,
                               std::string_view profile_id) const {
  const auto& basses = bass_options(quality, profile_id);
  const auto& rights = right_hand_options(quality, profile_id);

  if (basses.empty() || rights.empty()) {
    throw std::runtime_error("ChordVoicingEngine: triad voicings unavailable for quality '" +
                             std::string(quality_name(quality)) + "'");
  }

  const BassPattern* bass_choice = nullptr;
  const RHChord* right_choice = nullptr;

  if (preferred_bass.has_value()) {
    bass_choice = &bass(quality, *preferred_bass, profile_id);
  } else {
    bass_choice = &basses.front();
  }

  if (preferred_right.has_value()) {
    right_choice = &right_hand(quality, *preferred_right, profile_id);
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

const ChordVoicingEngine::Profile&
ChordVoicingEngine::profile_for(std::string_view profile_id) const {
  if (!profile_id.empty()) {
    auto it = profiles_.find(std::string(profile_id));
    if (it != profiles_.end()) {
      return it->second;
    }
  }

  auto fallback = profiles_.find(default_profile_id());
  if (fallback != profiles_.end()) {
    return fallback->second;
  }

  throw std::runtime_error("ChordVoicingEngine: default profile is not registered");
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
