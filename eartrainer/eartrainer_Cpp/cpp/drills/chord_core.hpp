#pragma once

#include "params.hpp"
#include "../include/ear/chord_voicings.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ear::drills::chord {

struct ChordSelectionState {
  std::optional<int> last_degree;
  std::optional<std::string> last_voicing_id;
  std::optional<int> last_top_degree;
  std::optional<int> last_top_midi;
};

struct TrainingRootConfig {
  bool enabled = false;
  double delay_beats = 0.0;
  int duration_ms = 0;
  int channel = 0;
  int program = 0;
  int velocity = 0;
};

struct ChordQuestionCore {
  int root_degree = 0;
  bool add_seventh = false;
  std::string quality;
  ear::ChordVoicingEngine::TriadQuality quality_enum = ear::ChordVoicingEngine::TriadQuality::Major;
  const ear::ChordVoicingEngine::RightHandPattern* right_pattern = nullptr;
  const ear::ChordVoicingEngine::BassPattern* bass_pattern = nullptr;
  std::string right_voicing_id;
  std::string bass_voicing_id;
  int bass_offset = 0;
  int bass_degree = 0;
  std::vector<int> right_degrees;
  int tonic_midi = 60;
  std::string profile_id;
  int top_degree = 0;
};

ChordQuestionCore prepare_chord_question(const DrillSpec& spec,
                                         int root_degree,
                                         std::uint64_t& rng_state,
                                         ChordSelectionState& selection_state,
                                         const std::optional<std::string>& preferred_right,
                                         const std::optional<std::string>& preferred_bass,
                                         std::string_view profile_id);

int select_bass_midi(const ChordQuestionCore& core);

std::vector<int> voice_right_hand_midi(const DrillSpec& spec,
                                       const ChordQuestionCore& core,
                                       int bass_midi,
                                       const std::optional<int>& previous_top_midi);

int find_voicing_index(const std::vector<ear::ChordVoicingEngine::RightHandPattern>& options,
                       const std::string& id);

TrainingRootConfig resolve_training_root_config(const DrillSpec& spec,
                                                bool split_tracks,
                                                int chord_duration_ms,
                                                int default_velocity,
                                                int right_channel_fallback,
                                                int merged_channel_fallback);

int adjust_helper_midi(int helper_midi, int bass_midi, const std::vector<int>& right_midi);

} // namespace ear::drills::chord
