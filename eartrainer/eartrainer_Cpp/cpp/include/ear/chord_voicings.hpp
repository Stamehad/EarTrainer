#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ear {

class ChordVoicingEngine {
public:
  enum class TriadQuality { Major = 0, Minor = 1, Diminished = 2 };

  struct BassPattern {
    std::string id;
    int degree_offset = 0;
  };

  struct RightHandPattern {
    std::string id;
    std::vector<int> degree_offsets;
  };

  struct Selection {
    const BassPattern* bass = nullptr;
    const RightHandPattern* right_hand = nullptr;
  };

  static const ChordVoicingEngine& instance();

  static const std::string& default_profile_id();
  const std::string& resolve_profile_id(std::string_view profile_id) const;

  const std::vector<BassPattern>& bass_options(TriadQuality quality,
                                               std::string_view profile_id = default_profile_id()) const;
  const std::vector<RightHandPattern>& right_hand_options(TriadQuality quality,
                                                          std::string_view profile_id = default_profile_id()) const;

  const BassPattern& bass(TriadQuality quality, const std::string& id,
                          std::string_view profile_id = default_profile_id()) const;
  const RightHandPattern& right_hand(TriadQuality quality, const std::string& id,
                                     std::string_view profile_id = default_profile_id()) const;

  Selection pick_triad(TriadQuality quality,
                       std::uint64_t& rng_state,
                       std::optional<std::string> preferred_right = std::nullopt,
                       std::optional<std::string> preferred_bass = std::nullopt,
                       std::optional<std::string> avoid_right = std::nullopt,
                       std::string_view profile_id = default_profile_id()) const;

private:
  ChordVoicingEngine();

  struct QualitySet {
    std::vector<BassPattern> bass;
    std::vector<RightHandPattern> right;
  };

  struct Profile {
    std::string id;
    QualitySet triads[3];
  };

  static std::size_t quality_index(TriadQuality quality);
  const Profile& profile_for(std::string_view profile_id) const;

  std::unordered_map<std::string, Profile> profiles_;
};

ChordVoicingEngine::TriadQuality triad_quality_from_string(const std::string& quality);

std::string to_string(ChordVoicingEngine::TriadQuality quality);

} // namespace ear
