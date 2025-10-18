#pragma once

#include "resources/builtin_make.hpp"
#include "resources/catalog_base.hpp"

#include <nlohmann/json.hpp>
#include <map>
#include <string_view>
#include <vector>

namespace ear::builtin::MelodyLevels {

inline constexpr std::string_view name = "melody_levels";

namespace detail {

struct Impl;

inline const std::vector<DrillSpec>& level_111() {
  static const std::vector<DrillSpec> drills = {
      make_drill("MELODY60_STEP1",
                 "melody",
                 111,
                 60,
                 nlohmann::json{{"melody_length", 2},
                                {"melody_max_step", 1},
                                {"range_below_semitones", 0},
                                {"range_above_semitones", 12}}),
      make_drill("MELODY60_STEP2",
                 "melody",
                 111,
                 60,
                 nlohmann::json{{"melody_length", 2},
                                {"melody_max_step", 2},
                                {"range_below_semitones", 0},
                                {"range_above_semitones", 12}}),
      make_drill("MELODY60",
                 "melody",
                 111,
                 60,
                 nlohmann::json{{"melody_length", 2},
                                {"range_below_semitones", 0},
                                {"range_above_semitones", 12}})};
  return drills;
}

inline const std::vector<DrillSpec>& level_112() {
  static const std::vector<DrillSpec> drills = {
      make_drill("MELODY90_STEP1",
                 "melody",
                 112,
                 90,
                 nlohmann::json{{"melody_length", jarray({2, 3})},
                                {"melody_max_step", 1},
                                {"range_below_semitones", 0},
                                {"range_above_semitones", 12}}),
      make_drill("MELODY90_STEP2",
                 "melody",
                 112,
                 60,
                 nlohmann::json{{"melody_length", jarray({2, 3})},
                                {"melody_max_step", 2},
                                {"range_below_semitones", 0},
                                {"range_above_semitones", 12}}),
      make_drill("MELODY90",
                 "melody",
                 112,
                 60,
                 nlohmann::json{{"melody_length", jarray({2, 3})},
                                {"range_below_semitones", 0},
                                {"range_above_semitones", 12}})};
  return drills;
}

inline const std::vector<DrillSpec>& level_113() {
  static const std::vector<DrillSpec> drills = {
      make_drill("MELODY120_STEP1",
                 "melody",
                 113,
                 90,
                 nlohmann::json{{"melody_length", jarray({2, 3, 4})},
                                {"melody_max_step", 1},
                                {"range_below_semitones", 0},
                                {"range_above_semitones", 12}}),
      make_drill("MELODY120_STEP2",
                 "melody",
                 113,
                 60,
                 nlohmann::json{{"melody_length", jarray({2, 3, 4})},
                                {"melody_max_step", 2},
                                {"range_below_semitones", 0},
                                {"range_above_semitones", 12}}),
      make_drill("MELODY120",
                 "melody",
                 113,
                 60,
                 nlohmann::json{{"melody_length", jarray({2, 3, 4})},
                                {"range_below_semitones", 0},
                                {"range_above_semitones", 12}})};
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
