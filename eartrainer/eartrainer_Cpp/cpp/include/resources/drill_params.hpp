#pragma once
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
  std::vector<int> allowed_degrees = {0,1,2,3,4,5,6};
  bool avoid_repeat = true;
  int range_below_semitones = 12;
  int range_above_semitones = 12;
  DrillInstrument inst = DrillInstrument::Piano;
  int tempo_bpm = 120;
  double note_beats = 1;
  int program = 0;
  int velocity = 96;
  
  // PATHWAY:
  bool use_pathway = false; 
  bool pathway_repeat_lead = false;
  double pathway_beats = 1.0;
  double pathway_rest = 1.0;
  
  // ANCHOR TONIC:
  enum class TonicAnchor {Before, After};
  double note_step_beats = 1.0;
  int note_tempo_bpm = 120;
  bool use_anchor = false;
  std::optional<TonicAnchor> tonic_anchor{}; // NO VALUE MEANS CHOOSE RANDOLMLY FOR EACH QUESTION
  bool tonic_anchor_include_octave = false;
};
struct IntervalParams {
  int tempo_bpm = 60;
  double note_beat = 1.0;
  int program = 0;
  std::vector<int> allowed_bottom_degrees{};
  std::vector<int> allowed_degrees = {0,1,2,3,4,5,6};
  std::vector<int> allowed_sizes{};
  bool avoid_repeat = true;
  int range_semitones = 12;
  int velocity = 96; 
  DrillInstrument inst = DrillInstrument::Piano;
};
struct MelodyParams {
  int tempo_bpm = 80;
  int program = 0 ;
  std::vector<int> melody_lengths = {3};
  int melody_max_step = 7;
  bool avoid_repeat = true;
  int range_below_semitones = 12;
  int range_above_semitones = 12;
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
}  // namespace ear
