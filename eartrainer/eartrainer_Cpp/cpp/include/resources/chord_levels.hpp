#pragma once

#include "resources/level_catalog.hpp"
#include "resources/catalog_base.hpp"

namespace ear::builtin::ChordLevels {

using namespace ear::builtin::catalog_numbered;

namespace build {
  using DP = ear::DrillParams;
  using CP = ear::ChordParams;
  using TRC = ear::ChordParams::TrainingRootConfig;
  using DI = ear::DrillInstrument;
  using VS = ear::VoicingsStyle;
  using TA = ear::ChordParams::TonicAnchor;
  //-------------------------------------------------
  // LEVEL 3 (WITH ROOT HELP)
  //-------------------------------------------------
  // STRING TRIADS WITH ROOT HELP
  inline DP d1300() {CP p; p.degrees = {1,4,5,7}; p.voicing_style = VS::Triad; p.play_root=TRC{.enabled=true}; return p;}
  inline DP d1301() {CP p; p.degrees = {1,2,3,6}; p.voicing_style = VS::Triad; p.play_root=TRC{.enabled=true}; return p;}
  inline DP d1305() {CP p; p.voicing_style = VS::Triad; p.play_root=TRC{.enabled=true}; return p;}
  // PIANO TRIADS WITH ROOT HELP
  inline DP d1310() {CP p; p.degrees = {1,4,5,7}; p.inst = DI::Piano; p.voicing_style = VS::Triad; p.play_root=TRC{.enabled=true}; return p;}
  inline DP d1311() {CP p; p.degrees = {1,2,3,6}; p.inst = DI::Piano; p.voicing_style = VS::Triad; p.play_root=TRC{.enabled=true}; return p;}
  inline DP d1315() {CP p; p.inst = DI::Piano; p.voicing_style = VS::Triad; p.play_root=TRC{.enabled=true}; return p;}
  // STRING VOICINGS WITH ROOT HELP
  inline DP d1320() {CP p; p.degrees = {1,4,5,7}; p.play_root=TRC{.enabled=true}; return p;}
  inline DP d1321() {CP p; p.degrees = {1,2,3,6}; p.play_root=TRC{.enabled=true}; return p;}
  inline DP d1325() {CP p; p.play_root=TRC{.enabled=true}; return p;}
  // PIANO VOICINGS WITH ROOT HELP
  inline DP d1330() {CP p; p.degrees = {1,4,5,7}; p.inst = DI::Piano; p.play_root=TRC{.enabled=true}; return p;}
  inline DP d1331() {CP p; p.degrees = {1,2,3,6}; p.inst = DI::Piano; p.play_root=TRC{.enabled=true}; return p;}
  inline DP d1335() {CP p; p.inst = DI::Piano; p.play_root=TRC{.enabled=true}; return p;}
  // MIXER
  inline DP d1390() {CP p; p.voicing_style = VS::Triad; p.play_root=TRC{.enabled=true}; return p;}
  inline DP d1391() {CP p; p.inst = DI::Piano; p.voicing_style = VS::Triad; p.play_root=TRC{.enabled=true}; return p;}
  inline DP d1392() {CP p; p.play_root=TRC{.enabled=true}; return p;}
  inline DP d1393() {CP p; p.inst = DI::Piano; p.play_root=TRC{.enabled=true}; return p;}

  //-------------------------------------------------
  // LEVEL 4 (NO HELP)
  //-------------------------------------------------
  // WARMUP
  inline DP d1400() {CP p; p.voicing_style = VS::Triad; p.play_root=TRC{.enabled=true}; return p;}
  inline DP d1401() {CP p; p.inst = DI::Piano; p.voicing_style = VS::Triad; p.play_root=TRC{.enabled=true}; return p;}
  // REFERENCE TONIC CHORD 
  inline DP d1410() {CP p; p.use_anchor=true; p.tonic_anchor=TA::Before; p.inst = DI::Piano; return p;}
  inline DP d1411() {CP p; p.use_anchor=true; p.tonic_anchor=TA::After; p.inst = DI::Piano; return p;}
  inline DP d1415() {CP p; p.use_anchor=true; p.inst = DI::Piano; return p;}
  // REFERENCE TONIC CHORD (STRINGS)
  inline DP d1420() {CP p; p.use_anchor=true; p.tonic_anchor=TA::Before; return p;}
  inline DP d1421() {CP p; p.use_anchor=true; p.tonic_anchor=TA::After; return p;}
  inline DP d1425() {CP p; p.use_anchor=true; return p;}
  // PIANO CHORDS
  inline DP d1430() {CP p; p.inst = DI::Piano; p.degrees = {1,4,5,7}; return p;}
  inline DP d1431() {CP p; p.inst = DI::Piano; p.degrees = {1,2,3,6}; return p;}
  inline DP d1435() {CP p; p.inst = DI::Piano; return p;}
  // STRINGS CHORDS
  inline DP d1440() {CP p; p.degrees = {1,4,5,7}; return p;}
  inline DP d1441() {CP p; p.degrees = {1,2,3,6}; return p;}
  inline DP d1445() {CP p; return p;}
  // MIXER
  inline DP d1490() {CP p; p.use_anchor=true; p.inst = DI::Piano; return p;}
  inline DP d1491() {CP p; p.inst = DI::Piano; return p;}
  inline DP d1492() {CP p; return p;}
  //-------------------------------------------------
  // LEVEL 5 (7TH CHORDS, N=2 BPM=60)
  //-------------------------------------------------

  //-------------------------------------------------
  // LEVEL 5 (INVERSIONS, TOP NOTE)
  //-------------------------------------------------

    
} // namespace build

using M = MetaData;

inline const std::vector<Lesson>& manifest() {
  static const std::vector<Lesson> Manifest = {
    // LEVEL 3
    Lesson{130, "C130: TRIADS WITH HELP", {
      {1300, &build::d1300, 10, "1,4,5,7"},
      {1301, &build::d1301, 10, "1,2,3,6"},
      {1305, &build::d1305, 20, "1-7"},
    }, LessonType::Lesson, M{.demote_to=-1, .promote_to=131, .mix=-1},},
    Lesson{131, "C131: PIANO TRIADS WITH HELP", {
      {1310, &build::d1310, 10, "1,4,5,7"},
      {1311, &build::d1311, 10, "1,2,3,6"},
      {1315, &build::d1315, 20, "1-7"},
    }, LessonType::Lesson, M{.demote_to=130, .promote_to=132, .mix=130},},
    Lesson{132, "C132: STRINGS WITH HELP", {
      {1320, &build::d1320, 10, "1,4,5,7"},
      {1321, &build::d1321, 10, "1,2,3,6"},
      {1325, &build::d1325, 20, "1-7"},
    }, LessonType::Lesson, M{.demote_to=131, .promote_to=133, .mix=130},},
    Lesson{133, "C133: PIANO WITH HELP", {
      {1330, &build::d1330, 10, "1,4,5,7"},
      {1331, &build::d1331, 10, "1,2,3,6"},
      {1335, &build::d1335, 20, "1-7"},
    }, LessonType::Lesson, M{.demote_to=132, .promote_to=134, .mix=131},},
    Lesson{139, "C139: MIXER", {
      {1390, &build::d1390, 15, "STRINGS TRIADS"},
      {1391, &build::d1391, 15, "PIANO TRIADS"},
      {1392, &build::d1392, 15, "STRINGS"},
      {1393, &build::d1393, 15, "PIANO"},
    }, LessonType::Mixer, M{.demote_to=130, .promote_to=140, .mix=-1},},
    // LEVEL 4
    Lesson{140, "C140: WARMUP", {
      {1400, &build::d1400, 15, "TRIAD WITH ROOT"},
      {1401, &build::d1401, 15, "PIANO TRIAD WITH ROOT"},
    }, LessonType::Warmup, M{.demote_to=139, .promote_to=141, .mix=-1}},
    Lesson{141, "C141: TONIC ANCHOR", {
      {1410, &build::d1410, 10, "BEFORE"},
      {1411, &build::d1411, 10, "AFTER"},
      {1415, &build::d1415, 20, "BEFORE OR AFTER"},
    }, LessonType::Lesson, M{.demote_to=140, .promote_to=142, .mix=133}},
    Lesson{142, "C142: TONIC ANCHOR STRINGS", {
      {1420, &build::d1420, 10, "BEFORE"},
      {1421, &build::d1421, 10, "AFTER"},
      {1425, &build::d1425, 20, "BEFORE OR AFTER"},
    }, LessonType::Lesson, M{.demote_to=141, .promote_to=143, .mix=132}},
    Lesson{143, "C143: PIANO CHORDS", {
      {1430, &build::d1430, 10, "1,4,5,7"},
      {1431, &build::d1431, 10, "1,2,3,6"},
      {1435, &build::d1435, 20, "1-7"},
    }, LessonType::Lesson, M{.demote_to=142, .promote_to=144, .mix=133}},
    Lesson{144, "C144: STRINGS CHORDS", {
      {1440, &build::d1440, 10, "1,4,5,7"},
      {1441, &build::d1441, 10, "1,2,3,6"},
      {1445, &build::d1445, 20, "1-7"},
    }, LessonType::Lesson, M{.demote_to=143, .promote_to=149, .mix=132}},
    Lesson{149, "C149: MIXER", {
      {1490, &build::d1490, 15, "PIANO TONIC ANCHOR"},
      {1491, &build::d1491, 15, "PIANO"},
      {1492, &build::d1492, 15, "STRINGS"},
    }, LessonType::Lesson, M{.demote_to=142, .promote_to=144, .mix=133}},
    
  };
  return Manifest;
}

} // namespace ear::builtin::ChordLevels
