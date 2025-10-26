#pragma once

#include "../include/ear/chord_voicings.hpp"
#include "drill.hpp"
#include "chord_core.hpp"

#include <optional>
#include <string>

namespace ear {

class ChordDrill : public DrillModule {
public:
  using TriadQuality = ::ear::TriadQuality;
  void configure(const DrillSpec& spec) override;
  QuestionBundle next_question(std::uint64_t& rng_state) override;

private:
  DrillSpec spec_{};
  ChordParams params{};
  DrillInstrument inst{};
  int tonic_midi;
  ChordVoicingEngine& v_engine = ChordVoicingEngine::instance();
  // ear::ChordVoicingEngine v_engine;
  drills::chord::ChordSelectionState selection_state_{};
  std::optional<std::string> preferred_right_voicing_;
  std::optional<std::string> preferred_bass_voicing_;
  std::string voicing_source_id_ = "builtin_diatonic_triads";
};

} // namespace ear
