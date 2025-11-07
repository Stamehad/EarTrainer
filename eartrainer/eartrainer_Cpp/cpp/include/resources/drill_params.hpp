#pragma once


#include "resources/schema.hpp"

#include <optional>
#include <string>
#include <variant>
#include <vector>
namespace ear {
enum class DrillParamKind {
  None = 0,
  Note,
  Interval,
  Melody,
  Chord,
};
enum class DrillInstrument {Piano, Strings /*, Guitar*/};
enum class ChordDelivery {Together, Arpeggio};

struct NoteParams {
  std::vector<int> degrees = {0,1,2,3,4,5,6};
  bool avoid_repeat = true;
  int range_down = 12; // semitones below tonic
  int range_up = 12; // semitones above tonic
  DrillInstrument inst = DrillInstrument::Piano;
  int bpm = 120;
  double note_beats = 1;
  int program = 0;
  int velocity = 96;
  
  // PATHWAY:
  bool pathway = false; 
  bool pathway_repeat_lead = false;
  double pathway_beats = 1.0;
  double pathway_rest = 1.0;
  bool incomplete = false;
  
  // ANCHOR TONIC:
  enum class TonicAnchor {Before, After};
  double note_step_beats = 1.0;
  int note_tempo_bpm = 120;
  bool use_anchor = false;
  std::optional<TonicAnchor> tonic_anchor{}; // NO VALUE MEANS CHOOSE RANDOLMLY FOR EACH QUESTION
  bool tonic_anchor_include_octave = false;
};
struct IntervalParams {
  int bpm = 60;
  double note_beat = 2.0;
  int program = 48;  // 0: PIANO, 48: STRINGS
  std::vector<int> allowed_bottom_degrees{};
  std::vector<int> allowed_degrees = {0,1,2,3,4,5,6};
  std::vector<int> intervals{};
  bool avoid_repeat = true;
  int range_semitones = 12;
  int velocity = 96; 
  DrillInstrument inst = DrillInstrument::Piano;
  std::vector<int> cluster_ids = {1,2,3,4,5,6};
  int helper = 0; // 0: no helper, 1: play ascending, -1: play descending, 2: play both
};
struct MelodyParams {
  int bpm = 80;
  int program = 0 ;
  std::vector<int> length = {3};
  std::vector<int> start = {0,1,2,3,4,5,6};
  int interval = 2; // 0: not intervallic, 1: up, -1: down, 2: both
  int max_step = 7;
  bool avoid_repeat = true;
  int range_down = 12;
  int range_up = 12;
  double note_beat = 1.0;
  int velocity = 96;
  DrillInstrument inst = DrillInstrument::Piano;
};
struct ChordParams {
  struct TrainingRootConfig {
    bool enabled = false;
    double delay_beats = 1.0;
    double dur_beats = 1.0;
    int channel = 0;
    int program = 0;
    int velocity = 0;
    int duration_ms = 1000; // LEGACY
  };
  
  std::vector<int> allowed_degrees = {0,1,2,3,4,5,6};
  DrillInstrument inst = DrillInstrument::Piano;
  ChordDelivery delivery = ChordDelivery::Together;
  std::vector<int> allowed_top_degrees{};
  bool avoid_repeat = true;
  std::optional<bool> chord_avoid_repeat{};
  int range_semitones = 12;
  bool add_seventh = false;
  int tempo_bpm = 120;
  
  // VOICINGS
  // struct VoicingConfig {
  // };
  std::string right_voicing_id{};
  std::string bass_voicing_id{};
  std::optional<std::string> voicing_profile{}; // "strings_ensemble"
  bool prompt_split_tracks = false;
  int prompt_program = 0; // -> PIANO, 48 -> STRINGS, GUITAR -> 24
  int prompt_channel = 0;
  int right_program = 0;
  int right_channel = 0;
  int bass_program = 0;
  int bass_channel = 1;
  int velocity = 96;
  double dur_beats = 2;
  int duration_ms = 1000; // LEGACY
  int strum_step_ms = 0;
  bool voice_leading_continuity = true;
  //std::optional<TrainingRootConfig> training_root{};
  TrainingRootConfig training_root{};
};
using DrillParams = std::variant<std::monostate, NoteParams, IntervalParams, MelodyParams, ChordParams>;

//===============================================================================
// PARAMETER SCHEMAS FOR UI GENERATION
//===============================================================================

inline const Schema& interval_schema() {
  static const Schema S{
    .id = "interval_params",
    .version = 1,
    .fields = {
      {"tempo_bpm", {"Tempo (BPM)", Kind::kInt, 60, IntRange{30,240,1}, {}, {}, "Playback tempo"}},
      {"note_beat", {"Note length (beats)", Kind::kDouble, 2.0, {}, RealRange{0.25,8.0,0.25}, {}, "Duration per note"}},
      {"program",   {"MIDI Program", Kind::kEnum, 48, {}, {}, {{"Piano",0},{"Strings",48}}, "Instrument program"}},
      {"allowed_bottom_degrees", {"Allowed bottom degrees", Kind::kIntList, std::vector<int>{}, {}, {}, {}, ""}},
      {"allowed_degrees", {"Allowed upper degrees", Kind::kIntList, std::vector<int>{0,1,2,3,4,5,6}, {}, {}, {}, ""}},
      {"allowed_sizes", {"Allowed interval sizes (semitones)", Kind::kIntList, std::vector<int>{}, {}, {}, {}, ""}},
      {"avoid_repeat", {"Avoid immediate repeats", Kind::kBool, true, {}, {}, {}, ""}},
      {"range_semitones", {"Pitch range (± semitones)", Kind::kInt, 12, IntRange{1,24,1}, {}, {}, ""}},
      {"velocity", {"Velocity", Kind::kInt, 96, IntRange{1,127,1}, {}, {}, ""}},
      {"inst", {"Instrument", Kind::kEnum, 0, {}, {}, {{"Piano",0},{"Strings",1}}, ""}},
      {"cluster_ids", {"Difficulty clusters", Kind::kIntList, std::vector<int>{1,2,3,4,5,6}, {}, {}, {}, ""}},
      {"add_helper", {"Play helper tone", Kind::kBool, false, {}, {}, {}, ""}},
    }
  };
  return S;
}

inline const Schema& note_schema() {
  static const Schema S{
    .id = "note_params",
    .version = 1,
    .fields = {
      {"allowed_degrees", {"Allowed degrees", Kind::kIntList, std::vector<int>{0,1,2,3,4,5,6}, {}, {}, {}, "Degrees relative to tonic (0-6)"}},
      {"avoid_repeat", {"Avoid immediate repeats", Kind::kBool, true, {}, {}, {}, ""}},
      {"range_below_semitones", {"Range below (semitones)", Kind::kInt, 12, IntRange{0,24,1}, {}, {}, ""}},
      {"range_above_semitones", {"Range above (semitones)", Kind::kInt, 12, IntRange{0,24,1}, {}, {}, ""}},
      {"inst", {"Instrument", Kind::kEnum, 0, {}, {}, {{"Piano",0},{"Strings",1}}, "Playback instrument"}},
      {"tempo_bpm", {"Tempo (BPM)", Kind::kInt, 120, IntRange{30,240,1}, {}, {}, "Playback tempo"}},
      {"note_beats", {"Note length (beats)", Kind::kDouble, 1.0, {}, RealRange{0.25,8.0,0.25}, {}, "Duration per note"}},
      {"program",   {"MIDI Program", Kind::kEnum, 0, {}, {}, {{"Piano",0},{"Strings",48}}, "Instrument program"}},
      {"velocity", {"Velocity", Kind::kInt, 96, IntRange{1,127,1}, {}, {}, ""}},

      // Pathway options
      {"use_pathway", {"Use pathway", Kind::kBool, false, {}, {}, {}, "Enable lead-in pathway"}},
      {"pathway_repeat_lead", {"Repeat lead note", Kind::kBool, false, {}, {}, {}, "Repeat first note in pathway"}},
      {"pathway_beats", {"Pathway note length (beats)", Kind::kDouble, 1.0, {}, RealRange{0.25,8.0,0.25}, {}, ""}},
      {"pathway_rest", {"Pathway rest (beats)", Kind::kDouble, 1.0, {}, RealRange{0.0,8.0,0.25}, {}, ""}},

      // Anchor tonic options
      {"note_step_beats", {"Step length (beats)", Kind::kDouble, 1.0, {}, RealRange{0.25,8.0,0.25}, {}, "Step duration for anchored motion"}},
      {"note_tempo_bpm", {"Anchor tempo (BPM)", Kind::kInt, 120, IntRange{30,240,1}, {}, {}, "Tempo when using anchor motion"}},
      {"use_anchor", {"Use anchor", Kind::kBool, false, {}, {}, {}, "Enable tonic anchor"}},
      // Represent optional enum as tri-state: -1 = random/unset, 0 = Before, 1 = After
      {"tonic_anchor", {"Tonic anchor position", Kind::kEnum, -1, {}, {}, {{"Random",-1},{"Before",0},{"After",1}}, "Unset/Random, or force anchor position"}},
      {"tonic_anchor_include_octave", {"Anchor includes octave", Kind::kBool, false, {}, {}, {}, "Double tonic with octave"}},
    }
  };
  return S;
}

inline const Schema& melody_schema() {
  static const Schema S{
    .id = "melody_params",
    .version = 1,
    .fields = {
      {"tempo_bpm", {"Tempo (BPM)", Kind::kInt, 80, IntRange{30,240,1}, {}, {}, "Playback tempo"}},
      {"program",   {"MIDI Program", Kind::kEnum, 0, {}, {}, {{"Piano",0},{"Strings",48}}, "Instrument program"}},
      {"melody_lengths", {"Melody lengths", Kind::kIntList, std::vector<int>{3}, {}, {}, {}, "Allowed lengths in notes"}},
      {"melody_max_step", {"Max step (semitones)", Kind::kInt, 7, IntRange{1,12,1}, {}, {}, "Maximum leap between notes"}},
      {"avoid_repeat", {"Avoid immediate repeats", Kind::kBool, true, {}, {}, {}, ""}},
      {"range_below_semitones", {"Range below (semitones)", Kind::kInt, 12, IntRange{0,24,1}, {}, {}, ""}},
      {"range_above_semitones", {"Range above (semitones)", Kind::kInt, 12, IntRange{0,24,1}, {}, {}, ""}},
      {"note_beat", {"Note length (beats)", Kind::kDouble, 1.0, {}, RealRange{0.25,8.0,0.25}, {}, "Duration per note"}},
      {"velocity", {"Velocity", Kind::kInt, 96, IntRange{1,127,1}, {}, {}, ""}},
      {"inst", {"Instrument", Kind::kEnum, 0, {}, {}, {{"Piano",0},{"Strings",1}}, "Playback instrument"}},
    }
  };
  return S;
}

inline const Schema& chord_schema() {
  static const Schema S{
    .id = "chord_params",
    .version = 1,
    .fields = {
      {"allowed_degrees", {"Allowed root degrees", Kind::kIntList, std::vector<int>{0,1,2,3,4,5,6}, {}, {}, {}, "Degrees relative to tonic (0-6)"}},
      {"inst", {"Instrument", Kind::kEnum, 0, {}, {}, {{"Piano",0},{"Strings",1}}, "Playback instrument"}},
      {"delivery", {"Delivery", Kind::kEnum, 0, {}, {}, {{"Together",0},{"Arpeggio",1}}, "How to play chord"}},
      {"allowed_top_degrees", {"Allowed top degrees", Kind::kIntList, std::vector<int>{}, {}, {}, {}, "Optional constraint for top voice"}},
      {"avoid_repeat", {"Avoid immediate repeats", Kind::kBool, true, {}, {}, {}, ""}},
      // Represent optional<bool> as tri-state: -1 = default, 0 = false, 1 = true
      {"chord_avoid_repeat", {"Chord avoid repeat (override)", Kind::kEnum, -1, {}, {}, {{"Default",-1},{"No",0},{"Yes",1}}, "Override per-chord repetition constraint"}},
      {"range_semitones", {"Pitch range (± semitones)", Kind::kInt, 12, IntRange{1,24,1}, {}, {}, ""}},
      {"add_seventh", {"Add seventh", Kind::kBool, false, {}, {}, {}, ""}},
      {"tempo_bpm", {"Tempo (BPM)", Kind::kInt, 120, IntRange{30,240,1}, {}, {}, "Playback tempo"}},

      // Voicing and prompt controls
      {"right_voicing_id", {"Right-hand voicing id", Kind::kString, std::string(""), {}, {}, {}, "Preset voicing id"}},
      {"bass_voicing_id", {"Bass voicing id", Kind::kString, std::string(""), {}, {}, {}, "Preset voicing id"}},
      {"voicing_profile", {"Voicing profile", Kind::kString, std::string(""), {}, {}, {}, "Named voicing profile (empty = default)"}},
      {"prompt_split_tracks", {"Split prompt tracks", Kind::kBool, false, {}, {}, {}, "Separate prompt track from answer"}},
      {"prompt_program",   {"Prompt program", Kind::kEnum, 0, {}, {}, {{"Piano",0},{"Strings",48}}, "MIDI program for prompt"}},
      {"prompt_channel", {"Prompt channel", Kind::kInt, 0, IntRange{0,15,1}, {}, {}, "MIDI channel"}},
      {"right_program",   {"Right program", Kind::kEnum, 0, {}, {}, {{"Piano",0},{"Strings",48}}, "MIDI program for right hand"}},
      {"right_channel", {"Right channel", Kind::kInt, 0, IntRange{0,15,1}, {}, {}, "MIDI channel"}},
      {"bass_program",   {"Bass program", Kind::kEnum, 0, {}, {}, {{"Piano",0},{"Strings",48}}, "MIDI program for bass"}},
      {"bass_channel", {"Bass channel", Kind::kInt, 1, IntRange{0,15,1}, {}, {}, "MIDI channel"}},
      {"velocity", {"Velocity", Kind::kInt, 96, IntRange{1,127,1}, {}, {}, ""}},
      {"dur_beats", {"Chord duration (beats)", Kind::kDouble, 2.0, {}, RealRange{0.25,8.0,0.25}, {}, "Duration for chord playback"}},
      {"duration_ms", {"Duration (ms, legacy)", Kind::kInt, 1000, IntRange{1,10000,1}, {}, {}, "Legacy duration override"}},
      {"strum_step_ms", {"Strum step (ms)", Kind::kInt, 0, IntRange{0,1000,1}, {}, {}, "Arpeggio step when strumming"}},
      {"voice_leading_continuity", {"Prefer voice-leading continuity", Kind::kBool, true, {}, {}, {}, "Preserve smooth motion between chords"}},

      // Training root flattened controls (no nested schema support here)
      {"training_root.enabled", {"Training root: enabled", Kind::kBool, false, {}, {}, {}, "Play tonic before chord"}},
      {"training_root.delay_beats", {"Training root: delay (beats)", Kind::kDouble, 1.0, {}, RealRange{0.0,8.0,0.25}, {}, ""}},
      {"training_root.dur_beats", {"Training root: duration (beats)", Kind::kDouble, 1.0, {}, RealRange{0.25,8.0,0.25}, {}, ""}},
      {"training_root.channel", {"Training root: channel", Kind::kInt, 0, IntRange{0,15,1}, {}, {}, "MIDI channel"}},
      {"training_root.program", {"Training root: program", Kind::kEnum, 0, {}, {}, {{"Piano",0},{"Strings",48}}, "MIDI program"}},
      {"training_root.velocity", {"Training root: velocity", Kind::kInt, 0, IntRange{0,127,1}, {}, {}, ""}},
      {"training_root.duration_ms", {"Training root: duration (ms)", Kind::kInt, 1000, IntRange{1,10000,1}, {}, {}, ""}},
    }
  };
  return S;
}


}  // namespace ear
