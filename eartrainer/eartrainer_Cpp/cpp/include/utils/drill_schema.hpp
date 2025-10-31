#pragma once
#include "include/utils/param_schema.hpp"
#include "drill_params.hpp"   // your IntervalParams + DrillInstrument

namespace ear {

inline const Schema<IntervalParams>& schema_for_interval() {
  static const Schema<IntervalParams> S{
    .id = "interval_params",
    .version = 1,
    .fields = {
      make_int<IntervalParams>("tempo_bpm","Tempo (BPM)", &IntervalParams::tempo_bpm, 30,240,1),
      make_double<IntervalParams>("note_beat","Note length (beats)", &IntervalParams::note_beat, 0.25,8.0,0.25),
      make_enum<IntervalParams, int>("program","MIDI Program", &IntervalParams::program,
                                     {{"Piano",0},{"Strings",48}}),
      make_vec_int<IntervalParams>("allowed_bottom_degrees","Allowed bottom degrees",
                                   &IntervalParams::allowed_bottom_degrees),
      make_vec_int<IntervalParams>("allowed_degrees","Allowed upper degrees",
                                   &IntervalParams::allowed_degrees),
      make_vec_int<IntervalParams>("allowed_sizes","Allowed interval sizes (semitones)",
                                   &IntervalParams::allowed_sizes),
      make_bool<IntervalParams>("avoid_repeat","Avoid immediate repeats",
                                &IntervalParams::avoid_repeat),
      make_int<IntervalParams>("range_semitones","Pitch range (± semitones)",
                               &IntervalParams::range_semitones, 1,24,1),
      make_int<IntervalParams>("velocity","Velocity",
                               &IntervalParams::velocity, 1,127,1),
      make_enum<IntervalParams, DrillInstrument>("inst","Instrument", &IntervalParams::inst,
                {{"Piano",(int)DrillInstrument::Piano},{"Strings",(int)DrillInstrument::Strings}}),
      make_vec_int<IntervalParams>("cluster_ids","Difficulty clusters",
                                   &IntervalParams::cluster_ids),
      make_bool<IntervalParams>("add_helper","Play helper tone",
                                &IntervalParams::add_helper),
    }
  };
  return S;
}

} // namespace ear