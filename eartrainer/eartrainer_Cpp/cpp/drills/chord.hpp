#pragma once

#include "../include/ear/chord_voicings.hpp"
#include "drill.hpp"
#include "chord_core.hpp"

#include <optional>
#include <string>

namespace ear {

class ChordDrill : public DrillModule {
public:
  void configure(const DrillSpec& spec) override;
  QuestionBundle next_question(std::uint64_t& rng_state) override;

private:
  DrillSpec spec_{};
  drills::chord::ChordSelectionState selection_state_{};
  std::optional<std::string> preferred_right_voicing_;
  std::optional<std::string> preferred_bass_voicing_;
  std::string voicing_source_id_ = "builtin_diatonic_triads";
};

class SustainChordDrill : public ChordDrill {
public:
  void configure(const DrillSpec& spec) override;
};

} // namespace ear
