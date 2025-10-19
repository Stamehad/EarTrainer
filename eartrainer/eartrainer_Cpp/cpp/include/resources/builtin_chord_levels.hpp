#pragma once

#include "resources/builtin_make.hpp"
#include "resources/catalog_base.hpp"

#include <nlohmann/json.hpp>
#include <map>
#include <string_view>
#include <vector>

namespace ear::builtin::ChordLevels {

inline constexpr std::string_view name = "chord_levels";

namespace detail {

struct Impl;

inline const std::vector<DrillSpec>& level_220() {
  static const std::vector<DrillSpec> drills = {
      make_drill("CHORD_TRIADS_PIANO",
                 "chord",
                 220,
                 90,
                 nlohmann::json{
                     {"chord_prompt_split_tracks", true},
                     {"chord_prompt_right_channel", 0},
                     {"chord_prompt_bass_channel", 1},
                     {"chord_prompt_program", 0},
                     {"chord_prompt_velocity", 90}}),
      make_drill("CHORD_TRIADS_STRINGS",
                 "chord_sustain",
                 220,
                 60,
                 nlohmann::json{
                     {"chord_voicing_profile", "strings_ensemble"},
                     {"chord_prompt_program", 48},
                     {"chord_prompt_duration_ms", 8000},
                     {"chord_prompt_velocity", 96}}),
      make_drill("CHORD_TRIADS_PIANO_ROOT_HELPER",
                 "chord",
                 220,
                 80,
                 nlohmann::json{
                     {"training_root_enabled", true},
                     {"training_root_delay_beats", 1.0},
                     {"training_root_channel", 2},
                     {"chord_prompt_split_tracks", true},
                     {"chord_prompt_right_channel", 0},
                     {"chord_prompt_bass_channel", 1},
                     {"chord_prompt_program", 0},
                     {"chord_prompt_velocity", 85}})};
  return drills;
}

inline const std::vector<DrillSpec>& level_221() {
  static const std::vector<DrillSpec> drills = {
      make_drill("CHORD_INVERSIONS_PIANO",
                 "chord",
                 221,
                 90,
                 nlohmann::json{
                     {"chord_prompt_split_tracks", true},
                     {"chord_prompt_program", 0},
                     {"chord_prompt_velocity", 88}}),
      make_drill("CHORD_INVERSIONS_STRINGS",
                 "chord_sustain",
                 221,
                 60,
                 nlohmann::json{
                     {"chord_voicing_profile", "strings_ensemble"},
                     {"chord_prompt_program", 49},
                     {"chord_prompt_velocity", 92},
                     {"chord_prompt_duration_ms", 10000}}),
      make_drill("CHORD_INVERSIONS_STRINGS_ROOT_HELPER",
                 "chord_sustain",
                 221,
                 60,
                 nlohmann::json{
                     {"training_root_enabled", true},
                     {"training_root_delay_beats", 0.75},
                     {"training_root_channel", 3},
                     {"chord_voicing_profile", "strings_ensemble"},
                     {"chord_prompt_program", 48},
                     {"chord_prompt_velocity", 92},
                     {"chord_prompt_duration_ms", 10000}})};
  return drills;
}

inline const std::vector<DrillSpec>& level_222() {
  static const std::vector<DrillSpec> drills = {
      make_drill("CHORD_EXTENDED_TRIADS",
                 "chord",
                 222,
                 80,
                 nlohmann::json{
                     {"add_seventh", true},
                     {"chord_prompt_split_tracks", true},
                     {"chord_prompt_program", 0},
                     {"chord_prompt_velocity", 88}}),
      make_drill("CHORD_EXTENDED_STRINGS",
                 "chord_sustain",
                 222,
                 60,
                 nlohmann::json{
                     {"add_seventh", true},
                     {"chord_voicing_profile", "strings_ensemble"},
                     {"chord_prompt_program", 48},
                     {"chord_prompt_velocity", 94},
                     {"chord_prompt_duration_ms", 12000}}),
      make_drill("CHORD_EXTENDED_PIANO_FAST",
                 "chord",
                 222,
                 110,
                 nlohmann::json{
                     {"add_seventh", true},
                     {"chord_prompt_split_tracks", true},
                     {"chord_prompt_program", 0},
                     {"chord_prompt_velocity", 90},
                     {"chord_prompt_duration_ms", 600}}),
      make_drill("CHORD_EXTENDED_STRINGS_ROOT_HELPER",
                 "chord_sustain",
                 222,
                 60,
                 nlohmann::json{
                     {"add_seventh", true},
                     {"training_root_enabled", true},
                     {"training_root_delay_beats", 0.5},
                     {"training_root_channel", 2},
                     {"chord_voicing_profile", "strings_ensemble"},
                     {"chord_prompt_program", 51},
                     {"chord_prompt_velocity", 94},
                     {"chord_prompt_duration_ms", 12000}})};
  return drills;
}

inline const std::vector<DrillSpec>& level_223() {
  static const std::vector<DrillSpec> drills = {
      make_drill("CHORD_STRINGS_LONG_SUSTAIN",
                 "chord_sustain",
                 223,
                 48,
                 nlohmann::json{
                     {"chord_voicing_profile", "strings_ensemble"},
                     {"chord_prompt_program", 52},
                     {"chord_prompt_velocity", 96},
                     {"chord_prompt_duration_ms", 15000},
                     {"training_root_enabled", false}}),
      make_drill("CHORD_STRINGS_FADING",
                 "chord_sustain",
                 223,
                 48,
                 nlohmann::json{
                     {"chord_voicing_profile", "strings_ensemble"},
                     {"chord_prompt_program", 48},
                     {"chord_prompt_velocity", 90},
                     {"chord_prompt_duration_ms", 9000},
                     {"training_root_enabled", true},
                     {"training_root_delay_beats", 1.5},
                     {"training_root_channel", 4}}),
      make_drill("CHORD_STRINGS_SMOOTH_TOP",
                 "chord_sustain",
                 223,
                 55,
                 nlohmann::json{
                     {"chord_voicing_profile", "strings_ensemble"},
                     {"chord_voice_leading_continuity", true},
                     {"chord_allowed_top_degrees", jarray({2, 4})},
                     {"chord_prompt_program", 48},
                     {"chord_prompt_velocity", 92},
                     {"chord_prompt_duration_ms", 12000}}),
      make_drill("CHORD_PIANO_WITH_ROOT_HINT",
                 "chord",
                 223,
                 72,
                 nlohmann::json{
                     {"training_root_enabled", true},
                     {"training_root_delay_beats", 1.0},
                     {"training_root_channel", 5},
                     {"chord_prompt_split_tracks", true},
                     {"chord_prompt_program", 0},
                     {"chord_prompt_velocity", 88},
                     {"chord_prompt_duration_ms", 900}}),
      make_drill("CHORD_PIANO_SMOOTH_TOP",
                 "chord",
                 223,
                 80,
                 nlohmann::json{
                     {"chord_voicing_profile", "simple_triads"},
                     {"chord_voice_leading_continuity", true},
                     {"chord_allowed_top_degrees", jarray({0, 2, 4})},
                     {"chord_prompt_split_tracks", true},
                     {"chord_prompt_program", 0},
                     {"chord_prompt_velocity", 85},
                     {"chord_prompt_duration_ms", 800}})};
  return drills;
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
