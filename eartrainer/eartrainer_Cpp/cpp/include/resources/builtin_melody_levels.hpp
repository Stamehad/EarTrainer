#pragma once

#include "ear/drill_spec.hpp"
#include "resources/drill_params.hpp"
#include "resources/catalog_base.hpp"

#include <map>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace ear::builtin::MelodyLevels {

inline constexpr std::string_view name = "melody_levels";

namespace detail {

struct Impl;

inline DrillSpec make_melody_drill(std::string id,
                                   int level,
                                   int tier,
                                   ear::MelodyParams params) {
  DrillSpec spec;
  spec.id = std::move(id);
  spec.family = "melody";
  spec.level = level;
  spec.tier = tier;
  spec.params = std::move(params);
  spec.apply_defaults();
  return spec;
}

inline const std::vector<DrillSpec>& level_111() {
  static const std::vector<DrillSpec> drills = [] {
    std::vector<DrillSpec> out;

    {
      ear::MelodyParams params;
      params.melody_lengths = {2};
      params.melody_max_step = 1;
      params.range_below_semitones = 0;
      params.range_above_semitones = 12;
      params.tempo_bpm = 60;
      out.push_back(make_melody_drill("MELODY60_STEP1", 111, 0, std::move(params)));
    }

    {
      ear::MelodyParams params;
      params.melody_lengths = {2};
      params.melody_max_step = 2;
      params.range_below_semitones = 0;
      params.range_above_semitones = 12;
      params.tempo_bpm = 60;
      out.push_back(make_melody_drill("MELODY60_STEP2", 111, 1, std::move(params)));
    }

    {
      ear::MelodyParams params;
      params.melody_lengths = {2};
      params.range_below_semitones = 0;
      params.range_above_semitones = 12;
      params.tempo_bpm = 60;
      out.push_back(make_melody_drill("MELODY60", 111, 2, std::move(params)));
    }

    return out;
  }();
  return drills;
}

inline const std::vector<DrillSpec>& level_112() {
  static const std::vector<DrillSpec> drills = [] {
    std::vector<DrillSpec> out;

    {
      ear::MelodyParams params;
      params.melody_lengths = {2, 3};
      params.melody_max_step = 1;
      params.range_below_semitones = 0;
      params.range_above_semitones = 12;
      params.tempo_bpm = 90;
      out.push_back(make_melody_drill("MELODY90_STEP1", 112, 0, std::move(params)));
    }

    {
      ear::MelodyParams params;
      params.melody_lengths = {2, 3};
      params.melody_max_step = 2;
      params.range_below_semitones = 0;
      params.range_above_semitones = 12;
      params.tempo_bpm = 60;
      out.push_back(make_melody_drill("MELODY90_STEP2", 112, 1, std::move(params)));
    }

    {
      ear::MelodyParams params;
      params.melody_lengths = {2, 3};
      params.range_below_semitones = 0;
      params.range_above_semitones = 12;
      params.tempo_bpm = 60;
      out.push_back(make_melody_drill("MELODY90", 112, 2, std::move(params)));
    }

    return out;
  }();
  return drills;
}

inline const std::vector<DrillSpec>& level_113() {
  static const std::vector<DrillSpec> drills = [] {
    std::vector<DrillSpec> out;

    {
      ear::MelodyParams params;
      params.melody_lengths = {2, 3, 4};
      params.melody_max_step = 1;
      params.range_below_semitones = 0;
      params.range_above_semitones = 12;
      params.tempo_bpm = 90;
      out.push_back(make_melody_drill("MELODY120_STEP1", 113, 0, std::move(params)));
    }

    {
      ear::MelodyParams params;
      params.melody_lengths = {2, 3, 4};
      params.melody_max_step = 2;
      params.range_below_semitones = 0;
      params.range_above_semitones = 12;
      params.tempo_bpm = 60;
      out.push_back(make_melody_drill("MELODY120_STEP2", 113, 1, std::move(params)));
    }

    {
      ear::MelodyParams params;
      params.melody_lengths = {2, 3, 4};
      params.range_below_semitones = 0;
      params.range_above_semitones = 12;
      params.tempo_bpm = 60;
      out.push_back(make_melody_drill("MELODY120", 113, 2, std::move(params)));
    }

    return out;
  }();
  return drills;
}

inline const std::vector<typename CatalogBase<Impl>::Row>& table() {
  static const std::vector<typename CatalogBase<Impl>::Row> rows = {
      {111, &level_111()},
      {112, &level_112()},
      {113, &level_113()},
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

} // namespace ear::builtin::MelodyLevels

