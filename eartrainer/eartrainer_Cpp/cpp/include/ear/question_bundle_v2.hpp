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
  KeyQuality key;                  // Major/Minor
  int root_degree;              // 0..6
  TriadQuality quality;         // derived, but useful for UI/telemetry
  // Optional labeling (no MIDI): stable IDs for UI display
  std::optional<std::vector<int>> rh_degrees;
  std::optional<int> bass_degrees;
  std::optional<std::string> right_voicing_id;
  std::optional<std::string> bass_voicing_id;
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
    int root_degree; 
    std::optional<int> bass_deg; // used for inversion
    std::optional<int> top_deg;  // query on top note
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
