#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

#include "resources/melody_levels.hpp"
#include "resources/harmony_levels.hpp"
#include "resources/chord_levels.hpp"
#include "resources/level_catalog.hpp"
#include "rng.hpp"

namespace ear::resources {

struct ManifestView {
  using Lesson = ear::builtin::catalog_numbered::Lesson;
  using MelodyList = std::vector<Lesson>;
  using HarmonyList = std::vector<Lesson>;
  using ChordList = std::vector<Lesson>;

  static constexpr int kTrackCount = 3;
  static constexpr std::array<const char*, kTrackCount> kTrackNames = {
      "melody", "harmony", "chord"};

  struct TrackPick {
    const Lesson* node = nullptr;
    int track_index = -1;
    std::vector<int> weights;
    std::vector<int> normalized_levels;
  };

  const MelodyList& melody;
  const HarmonyList& harmony;
  const ChordList& chords;

  const Lesson* melody_entry(int level) const {
    const auto it = std::find_if(melody.begin(), melody.end(),
                                 [level](const Lesson& node) { return node.lesson == level; });
    return it == melody.end() ? nullptr : &(*it);
  }

  const Lesson* harmony_entry(int level) const {
    const auto it = std::find_if(harmony.begin(), harmony.end(),
                                 [level](const Lesson& node) { return node.lesson == level; });
    return it == harmony.end() ? nullptr : &(*it);
  }

  const Lesson* chord_entry(int level) const {
    const auto it = std::find_if(chords.begin(), chords.end(),
                                 [level](const Lesson& node) { return node.lesson == level; });
    return it == chords.end() ? nullptr : &(*it);
  }

  const Lesson* entry(int level, const char* family) const {
    const std::string_view name{family};
    if (name == "melody") return melody_entry(level);
    if (name == "harmony") return harmony_entry(level);
    if (name == "chord") return chord_entry(level);
    return nullptr;
  }

  const Lesson* promote(const Lesson& node) const {
    if (node.meta.promote_to <= 0) {
      return nullptr;
    }
    return entry(node.meta.promote_to, node.family());
  }

  TrackPick pick_track(const std::vector<int>& levels, std::uint64_t& seed) const {
    TrackPick pick;
    std::array<int, kTrackCount> normalized{0, 0, 0};
    for (std::size_t i = 0; i < std::min<std::size_t>(levels.size(), kTrackCount); ++i) {
      normalized[i] = levels[i];
    }

    struct Candidate {
      const Lesson* node = nullptr;
      int track_index = -1;
      int weight = 0;
    };
    std::vector<Candidate> candidates;
    candidates.reserve(kTrackCount);
    std::array<int, kTrackCount> weights{0, 0, 0};

    const auto append_candidate = [&](const Lesson* node, int track_index) {
      if (!node) {
        return;
      }
      const int weight = weight_for(*node);
      candidates.push_back(Candidate{node, track_index, weight});
      if (track_index >= 0 && track_index < kTrackCount) {
        weights[track_index] = weight;
      }
    };

    if (!levels.empty() && levels[0] > 0) {
      append_candidate(melody_entry(levels[0]), 0);
    }
    if (levels.size() > 1 && levels[1] > 0) {
      append_candidate(harmony_entry(levels[1]), 1);
    }
    if (levels.size() > 2 && levels[2] > 0) {
      append_candidate(chord_entry(levels[2]), 2);
    }

    pick.normalized_levels.assign(normalized.begin(), normalized.end());
    pick.weights.assign(weights.begin(), weights.end());

    if (candidates.empty()) {
      if (!melody.empty()) {
        pick.node = &melody.front();
        pick.track_index = 0;
      } else if (!harmony.empty()) {
        pick.node = &harmony.front();
        pick.track_index = 1;
      } else if (!chords.empty()) {
        pick.node = &chords.front();
        pick.track_index = 2;
      }
      return pick;
    }

    long long total_weight = 0;
    for (const auto& candidate : candidates) {
      if (candidate.weight > 0) {
        total_weight += candidate.weight;
      }
    }

    if (total_weight <= 0) {
      pick.node = candidates.front().node;
      pick.track_index = candidates.front().track_index;
      return pick;
    }

    const long long draw = ear::rand_int(seed, 1, static_cast<int>(total_weight));
    long long cumulative = 0;
    for (const auto& candidate : candidates) {
      if (candidate.weight <= 0) {
        continue;
      }
      cumulative += candidate.weight;
      if (draw <= cumulative) {
        pick.node = candidate.node;
        pick.track_index = candidate.track_index;
        return pick;
      }
    }

    pick.node = candidates.back().node;
    pick.track_index = candidates.back().track_index;
    return pick;
  }

private:

  int weight_for(const Lesson& node) const {
    int weight = 1;
    const int level = node.get_level();
    const Lesson* current = &node;
    while (true) {
      const Lesson* next = promote(*current);
      if (!next || next->get_level() != level) {
        break;
      }
      ++weight;
      current = next;
    }
    return weight;
  }
};

inline ManifestView manifest() {
  return ManifestView{
      ear::builtin::MelodyLevels::manifest(),
      ear::builtin::HarmonyLevels::manifest(),
      ear::builtin::ChordLevels::manifest()};
}

} // namespace ear::resources
