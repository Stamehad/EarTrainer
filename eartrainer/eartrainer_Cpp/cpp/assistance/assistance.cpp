#include "assistance.hpp"

#include "../drills/common.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace ear::assistance {
namespace {

MidiClip make_tonic_clip(const SessionSpec& spec) {
  const int tempo = 90; // fixed assist tempo
  MidiClipBuilder builder(tempo, 480);
  const auto track = builder.add_track("tonic", 0, 0);

  const int tonic = drills::central_tonic_midi(spec.key);

  Beats beat{0.0};
  builder.add_note(track, beat, Beats{1.8}, tonic, std::nullopt);
  return builder.build();
}

MidiClip make_scale_arpeggio_clip(const SessionSpec& spec) {
  const int tempo = 90; // fixed assist tempo
  MidiClipBuilder builder(tempo, 480);
  const auto track = builder.add_track("scale", 0, 0);

  const std::vector<int> pattern = {0, 1, 2, 3, 4, 5, 6, 7, 4, 2, 0};
  const int tonic = drills::central_tonic_midi(spec.key);

  Beats beat{0.0};
  for (std::size_t i = 0; i < pattern.size(); ++i) {
    int degree = pattern[i];
    int midi = tonic + drills::degree_to_offset(degree);
    const double dur = (i == pattern.size() - 1) ? 1.08 : 0.72;
    builder.add_note(track, beat, Beats{dur}, midi, std::nullopt);
    beat.advance_by(dur);
  }
  return builder.build();
}

MidiClip make_scale_clip(const SessionSpec& spec) {
  const int tempo = 90; // fixed assist tempo
  MidiClipBuilder builder(tempo, 480);
  const auto track = builder.add_track("scale", 0, 0);

  const std::vector<int> pattern = {0, 1, 2, 3, 4, 5, 6, 7};
  const int tonic = drills::central_tonic_midi(spec.key);

  Beats beat{0.0};
  for (std::size_t i = 0; i < pattern.size(); ++i) {
    int degree = pattern[i];
    int midi = tonic + drills::degree_to_offset(degree);
    const double dur = (i == pattern.size() - 1) ? 1.08 : 0.72;
    builder.add_note(track, beat, Beats{dur}, midi, std::nullopt);
    beat.advance_by(dur);
  }
  return builder.build();
}

MidiClip make_cadence_clip(const SessionSpec& spec) {
  const int tempo = 90; // fixed assist tempo
  MidiClipBuilder builder(tempo, 480);
  const auto track = builder.add_track("cadence", 0, 0);

  const std::vector<int> pattern = {0, 1, 2, 3, 4, 5, 6, 7};
  const std::vector<std::vector<int>> cadence = {
    {-14, -7, 0},                // C2-C3-C4
    {-11, -4, 1, 3, 7},          // F2-F3-D4-A4-C5
    {-10, -3, 2, 4, 7},          // G2-G3-E4-G4-C5
    {-17, -10, 1, 3, 4, 6},      // G1-G2-D4-F4-G4-B4
    {-14, -7, 0, 2, 4, 7}        // C2-C3-C4-E4-G4-C5
  };
  const int tonic = drills::central_tonic_midi(spec.key);

  Beats beat{0.0};
  for (std::vector<int> chord : cadence) {
    std::vector<int> chord_midis; 
    chord_midis.reserve(chord.size());
    for (int degree : chord) {
      int midi = tonic + drills::degree_to_offset(degree);
      chord_midis.push_back(midi);
    }
    builder.add_chord(track, beat, Beats{1.0}, chord_midis, std::nullopt);
    beat.advance_by(1.0);
  }
  return builder.build();
}


std::string normalize_kind(const std::string& kind) {
  std::string normalized;
  normalized.reserve(kind.size());
  for (char ch : kind) {
    if (std::isalnum(static_cast<unsigned char>(ch))) {
      normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
  }
  return normalized;
}

const std::array<std::pair<const char*, MidiClip (*)(const SessionSpec&)>, 4>& registry() {
  static const std::array<std::pair<const char*, MidiClip (*)(const SessionSpec&)>, 4> defs = {{
      {"tonic", &make_tonic_clip},
      {"scalearpeggio", &make_scale_arpeggio_clip},
      {"scale", &make_scale_clip},
      {"cadence", &make_cadence_clip},
  }};
  return defs;
}

MidiClip dispatch_clip(const SessionSpec& spec, const std::string& kind) {
  const auto normalized = normalize_kind(kind);
  for (const auto& [token, factory] : registry()) {
    if (normalized == token) {
      return factory(spec);
    }
  }
  throw std::invalid_argument("Unknown assist kind: " + kind);
}

} // namespace

AssistBundle make_assist(const QuestionBundle& question, const std::string& kind) {
  AssistBundle bundle;
  bundle.question_id = question.question_id;
  bundle.kind = kind;
  bundle.prompt_clip = std::nullopt;
  return bundle;
}

std::vector<std::string> session_assist_kinds() {
  std::vector<std::string> kinds;
  kinds.reserve(registry().size());
  for (const auto& [token, _] : registry()) {
    if (std::string(token) == "tonic") {
      kinds.emplace_back("Tonic");
    } else if (std::string(token) == "scalearpeggio") {
      kinds.emplace_back("ScaleArpeggio");
    } else if (std::string(token) == "scale") {
      kinds.emplace_back("Scale");
    } else if (std::string(token) == "cadence") {
      kinds.emplace_back("Cadence");
    } else {
      kinds.emplace_back(token);
    }
  }
  return kinds;
}

MidiClip session_assist_clip(const SessionSpec& spec, const std::string& kind) {
  return dispatch_clip(spec, kind);
}

AssistBundle make_session_assist(const SessionSpec& spec, const std::string& kind) {
  AssistBundle bundle;
  bundle.question_id.clear();
  bundle.kind = kind;
  bundle.prompt_clip = session_assist_clip(spec, kind);
  return bundle;
}

MidiClip orientation_clip(const SessionSpec& spec) {
  return make_scale_arpeggio_clip(spec);
}

} // namespace ear::assistance
