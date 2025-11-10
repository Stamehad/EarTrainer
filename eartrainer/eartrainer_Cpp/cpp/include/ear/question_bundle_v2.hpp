#pragma once
// Ensure single inclusion to avoid redefinition errors across the tree
#pragma once

#include <optional>
#include <vector>

#include "midi_clip.hpp"
#include "chord_types.hpp"

namespace ear {

// Forward declaration to avoid circular include with types.hpp
enum class KeyQuality;

// Enums you already have or plan to have
enum class AnswerKind { ChordDegree /* ... */ };
enum class Delivery { Together, Arpeggio };
enum class Instrument { Piano, Strings, Guitar };

// --- Question payloads (typed, no playback) ---
struct ChordQuestionV2 {
  int tonic_midi;               // e.g., 60
  std::string tonic;            // e.g., C, Eb, F#
  KeyQuality key;               // Major/Minor
  std::vector<int> root_degrees;               // chord-by-chord root degrees
  std::vector<TriadQuality> qualities;         // triad quality per chord
  std::vector<std::optional<std::vector<int>>> rh_degrees; // per-chord RH degrees
  std::vector<std::optional<int>> bass_degrees;             // per-chord bass degree
  std::vector<std::optional<std::string>> right_voicing_ids;
  std::vector<std::optional<std::string>> bass_voicing_ids;
  std::vector<bool> is_anchor;                 // true when chord is a tonic anchor helper
};

struct MelodyQuestionV2 {
  int tonic_midi;               // e.g., 60
  std::string tonic;            // e.g., C, Eb, F#
  KeyQuality key;               // Major/Minor
  std::vector<int> melody;      // (n1,n2,...) in 0..6
  std::optional<std::vector<int>> octave;    // e.g. 0,1,-1 to specify octave of degree if needed
  std::optional<std::string> helper;         // e.g. pathways, tonic anchor 
};

struct HarmonyQuestionV2 {
    int tonic_midi;
    std::string tonic;            // e.g., C, Eb, F#
    KeyQuality key;               // Major/Minor
    int note_num; 
    std::vector<int> notes;
    std::optional<std::string> interval; // interval name (used only for note_num = 2)
};

struct ChordAnswerV2 { 
    std::vector<int> root_degrees;
    std::vector<std::optional<int>> bass_deg; // used for inversion
    std::vector<std::optional<int>> top_deg;  // query on top note
    std::vector<bool> expect_root;
    std::vector<bool> expect_bass;
    std::vector<bool> expect_top;
};

struct MelodyAnswerV2 {
    std::vector<int> melody;
};

struct HarmonyAnswerV2 {
    std::vector<int> notes;
};

using QuestionPayloadV2 = std::variant<ChordQuestionV2, MelodyQuestionV2, HarmonyQuestionV2>;
using AnswerPayloadV2   = std::variant<ChordAnswerV2, MelodyAnswerV2, HarmonyAnswerV2>;

// --- Slim UI hints (no catalogs) ---
struct UiHintsV2 {
  AnswerKind answer_kind = AnswerKind::ChordDegree;
  std::vector<std::string> allowed_assists;  // e.g., {"Replay","GuideTone"}
};

// --- The bundle built by drills (typed, JSON-free) ---
struct QuestionBundle {
  std::string       question_id;
  QuestionPayloadV2 question;
  AnswerPayloadV2   correct_answer;
  std::optional<ear::MidiClip> prompt_clip;  // factory will convert to MidiClip
  std::optional<UiHintsV2> ui_hints;
};

}
