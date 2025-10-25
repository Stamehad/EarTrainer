#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <numeric> // Required for std::accumulate
#include <iomanip> // Required for std::setprecision

#include "../include/ear/types.hpp"
#include "../resources/drill_params.hpp"
#include "../src/rng.hpp"

namespace ear {

class ChordVoicingEngine {
public:
  enum class TriadQuality { Major = 0, Minor = 1, Diminished = 2 };

  struct RightVoicing {
    std::string id;                 // stable voicing id (for UI/answer manifest)
    TriadQuality quality;           // enum (Major, Minor, Diminished, etc.)
    std::vector<int> degree_offsets;// e.g., {0, 2, 4} for triad
    std::vector<int> right_midi;    // absolute MIDI notes (post register shift)
    int top_midi;                   // convenience
  };

  struct BassChoice {
    std::string id;                 // stable bass voicing id
    int degree_offset;              // e.g., 0 for root, 2 for 3rd-in-bass
    int bass_degree;                // root_degree + degree_offset (mod 7)
    int bass_midi;                  // absolute MIDI (post register shift)
  };

  struct BassPattern {
    std::string id;
    int degree_offset = 0;
  };

  //---------------------------------------------------------------------------
  // HANDLE SCALE TYPES
  //---------------------------------------------------------------------------
  static constexpr int MAJOR_SCALE[7] = {0, 2, 4, 5, 7, 9, 11};
  static constexpr int MINOR_SCALE[7] = {0, 2, 3, 5, 7, 8, 10};

  static constexpr const int* get_scale_steps(KeyType keytype) {
      switch (keytype) {
          case KeyType::Major:
              return MAJOR_SCALE;
          case KeyType::Minor:
              return MINOR_SCALE;
          default:
              return MAJOR_SCALE;
      }
  }

  //---------------------------------------------------------------------------
  // RIGHT HAND CHORD VOICING
  //---------------------------------------------------------------------------
  struct RightHandPattern {
    std::string id;
    std::vector<int> degree_offsets;

    // METHOD: RELATIVE TO ROOT DEGREES -> RELATIVE TO TONIC
    RightHandPattern shift_to(int root_degree) const {
      std::vector<int> shifted;
      shifted.reserve(degree_offsets.size()); 
      for (int d : degree_offsets) {shifted.push_back(root_degree + d);}
      return RightHandPattern{id, shifted};
    }

    // METHOD: RELATIVE TO TONIC -> MIDI NOTE NUMBERS
    RightHandPattern to_midi(KeyType keytype, int tonic_midi, int midi_center=60) const {
      const int* scale_steps_ptr = get_scale_steps(keytype);
      std::vector<int> midis;
      midis.reserve(degree_offsets.size()); 
      
      for (int d : degree_offsets){
        int idx = d%7; if (idx < 0) idx+=7;
        int octave = (d-idx)/7;
        midis.push_back(scale_steps_ptr[static_cast<size_t>(idx)] + 12*octave + tonic_midi);
      }
      
      std::vector<int> new_midis = recenter_midis(midi_center, midis);
      return RightHandPattern{id, new_midis};
    }

    std::vector<int> recenter_midis(int midi_center, std::vector<int> midis) const {
      // MIDI CENTER OF MASS
      if (midis.empty()) {return midis; }
      double com = std::accumulate(midis.begin(), midis.end(), 0.0);
      com /= midis.size();
      
      double delta = com - midi_center; 
      if (std::abs(delta) <=6){return midis; }
      int shift = 0;
      int sign = (delta >= 0) ? -1 : 1;
      while (std::abs(delta) > 6){delta += sign * 12; shift += sign * 12; }

      std::vector<int> new_midis;
      new_midis.reserve(midis.size());
      for (int m : midis) {new_midis.push_back(m + shift);}
      return new_midis;
    }
    
    int get_top_degree() {
      return degree_offsets.back();
    }

    bool adjacent(int degree) {
      return (std::abs(get_top_degree() - degree) <= 1);
    }
  };

  //---------------------------------------------------------------------------
  // RIGHT HAND CHORD VOICINGS COLLECTION
  //---------------------------------------------------------------------------
  struct RightHandPatterns {
    public:
      std::vector<RightHandPattern> rh_patterns;

      // METHOD: RELATIVE TO ROOT DEGREES -> RELATIVE TO TONIC
      RightHandPatterns shift_to(int root_degree) const {
        std::vector<RightHandPattern> out;
        out.reserve(rh_patterns.size());

        std::transform(rh_patterns.begin(), rh_patterns.end(),
            std::back_inserter(out),
            [root_degree](const RightHandPattern& p) {return p.shift_to(root_degree);}
        );
        return RightHandPatterns{std::move(out)};
      }
      
      // METHOD: RELATIVE TO TONIC -> MIDI NOTE NUMBERS
      RightHandPatterns to_midi(KeyType keytype, int tonic_midi) const {
        std::vector<RightHandPattern> rh_midis;
        rh_midis.reserve(rh_patterns.size());

        std::transform(rh_patterns.begin(), rh_patterns.end(),
            std::back_inserter(rh_midis),
            [keytype, tonic_midi](const RightHandPattern& p) {return p.to_midi(keytype, tonic_midi);}
        );
        return RightHandPatterns{std::move(rh_midis)};
      }

      // METHOD: PRESERVE VOICE LEADING CONTINUITY
      // FILTER OUT VOICINGS WHERE TOP DEGREE IS NOT ADJACENT TO TOP DEGREE OF PREVIOUS CHORD
      RightHandPatterns filter_by_top_degree(int top_degree) {
        std::vector<RightHandPattern> filtered;
        filtered.reserve(rh_patterns.size());
        for (RightHandPattern p : rh_patterns){
          if (p.adjacent(top_degree)){filtered.push_back(p);}
        }
        if (filtered.empty()) return RightHandPatterns{ rh_patterns };
        return RightHandPatterns{std::move(filtered)};
      }

      // HELPER METHODS
      size_t size() {return rh_patterns.size();}
      RightHandPattern& operator[](size_t idx){return rh_patterns[idx]; }
      const RightHandPattern& operator[](size_t idx) const {return rh_patterns[idx]; }

      // SAMPLING
      RightHandPattern sample(std::uint64_t& rng_state) {
        int len = rh_patterns.size();
        int idx = rand_int(rng_state, 0, static_cast<int>(len) - 1);
        return rh_patterns[static_cast<std::size_t>(idx)];
      }
  };

  struct Selection {
    const BassPattern* bass = nullptr;
    const RightHandPattern* right_hand = nullptr;
  };

  static ChordVoicingEngine& instance();

  void configure(const KeyType& keytype, const DrillInstrument& inst, int tonic_midi, bool voice_leading_continuity); 

  RightVoicing get_voicing(int deg, std::uint64_t& rng_state);

  BassChoice get_bass(int deg, std::uint64_t& rng_state);

  RightHandPatterns get_ps();

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

  KeyType keytype_{};
  DrillInstrument inst_{};
  int tonic_midi_{};
  bool continuity_{};
  std::optional<int> top_degree_{};
  std::optional<int> top_midi_{};

  TriadQuality degree_to_quality(int degree){
    size_t d = degree%7;
    using T = TriadQuality;
    std::vector<T> major_qaulities = {T::Major, T::Minor, T::Minor, T::Major, T::Major, T::Minor, T::Diminished};
    std::vector<T> minor_qaulities = {T::Minor, T::Diminished, T::Major, T::Minor, T::Minor, T::Major, T::Major};
    switch (keytype_){
      case KeyType::Major:
        return major_qaulities[d];
      case KeyType::Minor:
        return minor_qaulities[d];
      default:
        return major_qaulities[d];
    }
  };

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
