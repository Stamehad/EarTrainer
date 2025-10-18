#pragma once

#include "resources/builtin_make.hpp"
#include "resources/catalog_base.hpp"

#include <nlohmann/json.hpp>
#include <map>
#include <string_view>
#include <vector>

namespace ear::builtin::DegreeLevels {

inline constexpr std::string_view name = "degree_levels";

namespace detail {

struct Impl;

inline const std::vector<DrillSpec>& level_1() {
  static const std::vector<DrillSpec> drills = {
      make_drill("PATHWAYS_1_4",
                 "note",
                 1,
                 80,
                 nlohmann::json{{"avoid_repeat", true},
                                {"allowed_degrees", jarray({0, 1, 2, 3})},
                                {"range_below_semitones", 0},
                                {"range_above_semitones", 12},
                                {"use_pathway", true},
                                {"pathway_repeat_lead", false},
                                {"pathway_step_beats", 1.0},
                                {"pathway_rest_beats", 0.5}}),
      make_drill("PATHWAYS_5_7",
                 "note",
                 1,
                 80,
                 nlohmann::json{{"avoid_repeat", true},
                                {"allowed_degrees", jarray({4, 5, 6})},
                                {"range_below_semitones", 0},
                                {"range_above_semitones", 12},
                                {"use_pathway", true},
                                {"pathway_repeat_lead", false},
                                {"pathway_step_beats", 1.0},
                                {"pathway_rest_beats", 0.5}})};
  return drills;
}

inline const std::vector<DrillSpec>& level_2() {
  static const std::vector<DrillSpec> drills = {
      make_drill("NOTE_1_4",
                 "note",
                 2,
                 72,
                 nlohmann::json{{"avoid_repeat", true},
                                {"allowed_degrees", jarray({0, 1, 2, 3})},
                                {"range_below_semitones", 0},
                                {"range_above_semitones", 12}}),
      make_drill("NOTE_5_7",
                 "note",
                 2,
                 72,
                 nlohmann::json{{"avoid_repeat", true},
                                {"allowed_degrees", jarray({4, 5, 6})},
                                {"range_below_semitones", 0},
                                {"range_above_semitones", 11}})};
  return drills;
}

inline const std::vector<DrillSpec>& level_3() {
  static const std::vector<DrillSpec> drills = {
      make_drill("NOTE_TO_TONIC",
                 "note",
                 3,
                 72,
                 nlohmann::json{{"avoid_repeat", true},
                                {"allowed_degrees", jarray({1, 2, 3, 4, 5, 6})},
                                {"tonic_anchor", "after"},
                                {"range_below_semitones", 0},
                                {"range_above_semitones", 12}}),
      make_drill("NOTE_AFTER_TONIC",
                 "note",
                 3,
                 72,
                 nlohmann::json{{"avoid_repeat", true},
                                {"allowed_degrees", jarray({1, 2, 3, 4, 5, 6})},
                                {"tonic_anchor", "before"},
                                {"range_below_semitones", 0},
                                {"range_above_semitones", 12}})};
  return drills;
}

inline const std::vector<DrillSpec>& level_11() {
  static const std::vector<DrillSpec> drills = {
      make_drill("PATHWAYS_2OCT",
                 "note",
                 11,
                 120,
                 nlohmann::json{{"avoid_repeat", true},
                                {"range_below_semitones", 12},
                                {"range_above_semitones", 12},
                                {"use_pathway", true},
                                {"pathway_repeat_lead", false},
                                {"pathway_step_beats", 1.0},
                                {"pathway_rest_beats", 0.5}}),
      make_drill("NOTE_WITH_TONIC_2OCT",
                 "note",
                 11,
                 120,
                 nlohmann::json{{"avoid_repeat", true},
                                {"allowed_degrees", jarray({1, 2, 3, 4, 5, 6})},
                                {"tonic_anchor", "both"},
                                {"tonic_anchor_include_octave", true},
                                {"range_below_semitones", 12},
                                {"range_above_semitones", 12}}),
      make_drill("NOTE_2OCT",
                 "note",
                 11,
                 72,
                 nlohmann::json{{"avoid_repeat", true},
                                {"range_below_semitones", 12},
                                {"range_above_semitones", 12}})};
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
