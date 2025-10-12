#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "ear/drill_spec.hpp"

namespace ear {

struct ChordVoicingLibrary {
  std::unordered_map<std::string, std::vector<int>> bass_offsets;
  std::unordered_map<std::string, std::vector<std::vector<int>>> right_hand;
};

ChordVoicingLibrary default_chord_voicings();

// Attempts to load chord voicings based on drill spec parameters. Returns the
// resolved absolute file path on success; otherwise std::nullopt and the library
// remains unchanged (aside from any partial updates applied before failure).
std::optional<std::filesystem::path> configure_chord_voicings(const DrillSpec& spec,
                                                              ChordVoicingLibrary& library);

} // namespace ear
