#pragma once

#include "ear/drill_spec.hpp"
#include "resources/drill_params.hpp"
#include "resources/catalog_base.hpp"

#include <map>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace ear::builtin::ChordLevels {

inline constexpr std::string_view name = "chord_levels";

namespace detail {

struct Impl;

inline DrillSpec make_chord_drill(std::string id,
                                  std::string family,
                                  int level,
                                  int tier,
                                  ear::ChordParams params) {
  DrillSpec spec;
  spec.id = std::move(id);
  spec.family = std::move(family);
  spec.level = level;
  spec.tier = tier;
  spec.params = std::move(params);
  return spec;
}

inline const std::vector<DrillSpec>& level_220() {
  static const std::vector<DrillSpec> drills = [] {
    std::vector<DrillSpec> out;

    {
      ear::ChordParams params;
      params.prompt_split_tracks = true;
      params.right_channel = 0;
      params.bass_channel = 1;
      params.prompt_program = 0;
      params.velocity = 90;
      out.push_back(make_chord_drill("CHORD_TRIADS_PIANO", "chord", 220, 0, std::move(params)));
    }

    {
      ear::ChordParams params;
      params.voicing_profile = "strings_ensemble";
      params.prompt_program = 48;
      params.velocity = 96;
      params.duration_ms = 8000;
      out.push_back(make_chord_drill("CHORD_TRIADS_STRINGS", "chord_sustain", 220, 1,
                                      std::move(params)));
    }

    {
      ear::ChordParams params;
      params.prompt_split_tracks = true;
      params.right_channel = 0;
      params.bass_channel = 1;
      params.prompt_program = 0;
      params.velocity = 85;
      params.training_root = ear::ChordParams::TrainingRootConfig{};
      auto& root = params.training_root;
      root.enabled = true;
      root.delay_beats = 1.0;
      root.channel = 2;
      out.push_back(make_chord_drill("CHORD_TRIADS_PIANO_ROOT_HELPER", "chord", 220, 2,
                                      std::move(params)));
    }

    return out;
  }();
  return drills;
}

inline const std::vector<DrillSpec>& level_221() {
  static const std::vector<DrillSpec> drills = [] {
    std::vector<DrillSpec> out;

    {
      ear::ChordParams params;
      params.prompt_split_tracks = true;
      params.prompt_program = 0;
      params.velocity = 88;
      out.push_back(make_chord_drill("CHORD_INVERSIONS_PIANO", "chord", 221, 0,
                                      std::move(params)));
    }

    {
      ear::ChordParams params;
      params.voicing_profile = "strings_ensemble";
      params.prompt_program = 49;
      params.velocity = 92;
      params.duration_ms = 10000;
      out.push_back(make_chord_drill("CHORD_INVERSIONS_STRINGS", "chord_sustain", 221, 1,
                                      std::move(params)));
    }

    {
      ear::ChordParams params;
      params.voicing_profile = "strings_ensemble";
      params.prompt_program = 48;
      params.velocity = 92;
      params.duration_ms = 10000;
      params.training_root = ear::ChordParams::TrainingRootConfig{};
      auto& root = params.training_root;
      root.enabled = true;
      root.delay_beats = 0.75;
      root.channel = 3;
      out.push_back(make_chord_drill("CHORD_INVERSIONS_STRINGS_ROOT_HELPER", "chord_sustain",
                                      221, 2, std::move(params)));
    }

    return out;
  }();
  return drills;
}

inline const std::vector<DrillSpec>& level_222() {
  static const std::vector<DrillSpec> drills = [] {
    std::vector<DrillSpec> out;

    {
      ear::ChordParams params;
      params.add_seventh = true;
      params.prompt_split_tracks = true;
      params.prompt_program = 0;
      params.velocity = 88;
      out.push_back(make_chord_drill("CHORD_EXTENDED_TRIADS", "chord", 222, 0,
                                      std::move(params)));
    }

    {
      ear::ChordParams params;
      params.add_seventh = true;
      params.voicing_profile = "strings_ensemble";
      params.prompt_program = 48;
      params.velocity = 94;
      params.duration_ms = 12000;
      out.push_back(make_chord_drill("CHORD_EXTENDED_STRINGS", "chord_sustain", 222, 1,
                                      std::move(params)));
    }

    {
      ear::ChordParams params;
      params.add_seventh = true;
      params.prompt_split_tracks = true;
      params.prompt_program = 0;
      params.velocity = 90;
      params.duration_ms = 600;
      out.push_back(make_chord_drill("CHORD_EXTENDED_PIANO_FAST", "chord", 222, 2,
                                      std::move(params)));
    }

    {
      ear::ChordParams params;
      params.add_seventh = true;
      params.voicing_profile = "strings_ensemble";
      params.prompt_program = 51;
      params.velocity = 94;
      params.duration_ms = 3000;
      params.voice_leading_continuity = true;
      params.training_root = ear::ChordParams::TrainingRootConfig{};
      auto& root = params.training_root;
      root.enabled = true;
      root.delay_beats = 1.0;
      root.channel = 2;
      out.push_back(make_chord_drill("CHORD_EXTENDED_STRINGS_ROOT_HELPER", "chord_sustain",
                                      222, 3, std::move(params)));
    }

    return out;
  }();
  return drills;
}

inline const std::vector<DrillSpec>& level_223() {
  static const std::vector<DrillSpec> drills = [] {
    std::vector<DrillSpec> out;

    {
      ear::ChordParams params;
      params.voicing_profile = "strings_ensemble";
      params.prompt_program = 52;
      params.velocity = 96;
      params.duration_ms = 15000;
      out.push_back(make_chord_drill("CHORD_STRINGS_LONG_SUSTAIN", "chord_sustain", 223, 0,
                                      std::move(params)));
    }

    {
      ear::ChordParams params;
      params.voicing_profile = "strings_ensemble";
      params.prompt_program = 48;
      params.velocity = 90;
      params.duration_ms = 9000;
      params.training_root = ear::ChordParams::TrainingRootConfig{};
      auto& root = params.training_root;
      root.enabled = true;
      root.delay_beats = 1.5;
      root.channel = 4;
      out.push_back(make_chord_drill("CHORD_STRINGS_FADING", "chord_sustain", 223, 1,
                                      std::move(params)));
    }

    {
      ear::ChordParams params;
      params.voicing_profile = "strings_ensemble";
      params.voice_leading_continuity = true;
      params.allowed_top_degrees = {2, 4};
      params.prompt_program = 48;
      params.velocity = 92;
      params.duration_ms = 12000;
      out.push_back(make_chord_drill("CHORD_STRINGS_SMOOTH_TOP", "chord_sustain", 223, 2,
                                      std::move(params)));
    }

    {
      ear::ChordParams params;
      params.prompt_split_tracks = true;
      params.prompt_program = 0;
      params.velocity = 88;
      params.duration_ms = 900;
      params.training_root = ear::ChordParams::TrainingRootConfig{};
      auto& root = params.training_root;
      root.enabled = true;
      root.delay_beats = 1.0;
      root.channel = 5;
      out.push_back(make_chord_drill("CHORD_PIANO_WITH_ROOT_HINT", "chord", 223, 3,
                                      std::move(params)));
    }

    {
      ear::ChordParams params;
      params.voicing_profile = "simple_triads";
      params.voice_leading_continuity = true;
      params.allowed_top_degrees = {0, 2, 4};
      params.prompt_split_tracks = true;
      params.prompt_program = 0;
      params.velocity = 85;
      params.duration_ms = 800;
      out.push_back(make_chord_drill("CHORD_PIANO_SMOOTH_TOP", "chord", 223, 4,
                                      std::move(params)));
    }

    return out;
  }();
  return drills;
}

struct GeneratorRow {
  int level;
  const std::vector<DrillSpec>& (*generator)();
};

inline const std::vector<GeneratorRow>& generator_table() {
  static const std::vector<GeneratorRow> rows = {
      {220, level_220},
      {221, level_221},
      {222, level_222},
      {223, level_223},
  };
  return rows;
}

inline const std::vector<typename CatalogBase<Impl>::Row>& table() {
  static const std::vector<typename CatalogBase<Impl>::Row> rows = {
      {220, &level_220()},
      {221, &level_221()},
      {222, &level_222()},
      {223, &level_223()},
  };
  return rows;
}

struct Impl : CatalogBase<Impl> {
  static const std::vector<typename CatalogBase<Impl>::Row>& table() { return detail::table(); }
};

} // namespace detail

inline const std::vector<int>& known_levels()                    { return detail::Impl::known_levels(); }
inline const std::map<int, std::vector<int>>& phases()           { return detail::Impl::phases(); }
inline const std::vector<DrillSpec>& drills_for_level(int level) { return detail::Impl::drills_for_level(level); }

} // namespace ear::builtin::ChordLevels
