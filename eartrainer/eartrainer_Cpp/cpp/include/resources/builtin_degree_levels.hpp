#pragma once

#include "ear/drill_spec.hpp"
#include "resources/drill_params.hpp"
#include "resources/catalog_base.hpp"

#include <map>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace ear::builtin::DegreeLevels {

inline constexpr std::string_view name = "degree_levels";

namespace detail {

struct Impl;

inline DrillSpec make_note_drill(std::string id,
                                 int level,
                                 int tier,
                                 ear::NoteParams params) {
  DrillSpec spec;
  spec.id = std::move(id);
  spec.family = "note";
  spec.level = level;
  spec.tier = tier;
  spec.params = std::move(params);
  spec.apply_defaults();
  return spec;
}

inline const std::vector<DrillSpec>& level_1() {
  static const std::vector<DrillSpec> drills = [] {
    std::vector<DrillSpec> out;

    {
      ear::NoteParams params;
      params.allowed_degrees = {0, 1, 2, 3};
      params.avoid_repeat = true;
      params.range_below_semitones = 0;
      params.range_above_semitones = 12;
      params.use_pathway = true;
      params.pathway_repeat_lead = false;
      params.tempo_bpm = 60;
      params.pathway_beats = 1.0;
      params.pathway_rest = 0.5;
      out.push_back(make_note_drill("PATHWAYS_1_4", 1, 0, std::move(params)));
    }

    {
      ear::NoteParams params;
      params.allowed_degrees = {4, 5, 6};
      params.avoid_repeat = true;
      params.range_below_semitones = 0;
      params.range_above_semitones = 12;
      params.use_pathway = true;
      params.pathway_repeat_lead = false;
      params.tempo_bpm = 60;
      params.pathway_beats = 1.0;
      params.pathway_rest = 0.5;
      out.push_back(make_note_drill("PATHWAYS_5_7", 1, 1, std::move(params)));
    }

    return out;
  }();
  return drills;
}

inline const std::vector<DrillSpec>& level_2() {
  static const std::vector<DrillSpec> drills = [] {
    std::vector<DrillSpec> out;

    {
      ear::NoteParams params;
      params.allowed_degrees = {0, 1, 2, 3};
      params.avoid_repeat = true;
      params.range_below_semitones = 0;
      params.range_above_semitones = 12;
      out.push_back(make_note_drill("NOTE_1_4", 2, 0, std::move(params)));
    }

    {
      ear::NoteParams params;
      params.allowed_degrees = {4, 5, 6};
      params.avoid_repeat = true;
      params.range_below_semitones = 0;
      params.range_above_semitones = 11;
      out.push_back(make_note_drill("NOTE_5_7", 2, 1, std::move(params)));
    }

    return out;
  }();
  return drills;
}

inline const std::vector<DrillSpec>& level_3() {
  static const std::vector<DrillSpec> drills = [] {
    std::vector<DrillSpec> out;

    {
      ear::NoteParams params;
      params.allowed_degrees = {1, 2, 3, 4, 5, 6};
      params.avoid_repeat = true;
      params.range_below_semitones = 0;
      params.range_above_semitones = 12;
      params.tonic_anchor = NoteParams::TonicAnchor::After;
      params.tonic_anchor_include_octave = false;
      params.note_tempo_bpm = 60;
      out.push_back(make_note_drill("NOTE_TO_TONIC", 3, 0, std::move(params)));
    }

    {
      ear::NoteParams params;
      params.allowed_degrees = {1, 2, 3, 4, 5, 6};
      params.avoid_repeat = true;
      params.range_below_semitones = 0;
      params.range_above_semitones = 12;
      params.tonic_anchor = NoteParams::TonicAnchor::Before;
      params.tonic_anchor_include_octave = false;
      params.note_tempo_bpm = 60;
      out.push_back(make_note_drill("NOTE_AFTER_TONIC", 3, 1, std::move(params)));
    }

    return out;
  }();
  return drills;
}

inline const std::vector<DrillSpec>& level_11() {
  static const std::vector<DrillSpec> drills = [] {
    std::vector<DrillSpec> out;

    {
      ear::NoteParams params;
      params.avoid_repeat = true;
      params.range_below_semitones = 12;
      params.range_above_semitones = 12;
      params.use_pathway = true;
      params.pathway_repeat_lead = false;
      params.tempo_bpm = 80;
      params.pathway_beats = 1.0;
      params.pathway_rest = 0.5;
      out.push_back(make_note_drill("PATHWAYS_2OCT", 11, 0, std::move(params)));
    }

    {
      ear::NoteParams params;
      params.allowed_degrees = {1, 2, 3, 4, 5, 6};
      params.avoid_repeat = true;
      params.range_below_semitones = 12;
      params.range_above_semitones = 12;
      params.tonic_anchor = NoteParams::TonicAnchor::Before;
      params.tonic_anchor_include_octave = true;
      params.note_tempo_bpm = 80;
      out.push_back(make_note_drill("NOTE_WITH_TONIC_2OCT", 11, 1, std::move(params)));
    }

    {
      ear::NoteParams params;
      params.avoid_repeat = true;
      params.range_below_semitones = 12;
      params.range_above_semitones = 12;
      out.push_back(make_note_drill("NOTE_2OCT", 11, 2, std::move(params)));
    }

    return out;
  }();
  return drills;
}

inline const std::vector<typename CatalogBase<Impl>::Row>& table() {
  static const std::vector<typename CatalogBase<Impl>::Row> rows = {
      {1, &level_1()},
      {2, &level_2()},
      {3, &level_3()},
      {11, &level_11()},
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

} // namespace ear::builtin::DegreeLevels
