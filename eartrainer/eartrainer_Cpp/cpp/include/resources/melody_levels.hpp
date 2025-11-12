#pragma once
#include "resources/level_catalog.hpp"

// #include "ear/drill_spec.hpp"
// #include "resources/drill_params.hpp"

// #include <map>
// #include <string>
// #include <string_view>
// #include <unordered_map>
// #include <utility>
// #include <vector>

namespace ear::builtin::MelodyLevels {

using namespace ear::builtin::catalog_numbered;
// -------------------------
// Param builders (examples)
// -------------------------
namespace build {
  using DP = ear::DrillParams;
  using NP = ear::NoteParams;
  using MP = ear::MelodyParams;
  //----------------------------------
  // LEVEL 0
  //----------------------------------
  inline DP d10() {NP p; p.pathway = true; p.bpm = 90; p.degrees = {0,1,2,3}; p.range_down = 0; return p;}
  inline DP d11() {NP p; p.pathway = true; p.bpm = 90; p.range_down = -2; p.degrees = {4,5,6,7}; return p;} // -2 is to avoid 7->0 swaps
  inline DP d15() {NP p; p.pathway = true; p.bpm = 90; p.range_down = 0; return p;}
  //
  inline DP d20() {NP p; p.pathway = true; p.bpm = 120; p.range_down = 0; p.degrees = {0,1,2,3}; return p;}
  inline DP d21() {NP p; p.pathway = true; p.bpm = 120; p.range_down = -2; p.degrees = {4,5,6,7}; return p;}
  inline DP d25() {NP p; p.pathway = false; p.bpm = 120; p.range_down = 0; return p;}
  //
  inline DP d30() {NP p; p.pathway = true; p.bpm = 120; p.range_down = 0; p.incomplete = true; p.degrees = {0,1,2,3}; return p;}
  inline DP d31() {NP p; p.pathway = true; p.bpm = 120; p.range_down = -2; p.incomplete = true; p.degrees = {4,5,6,7}; return p;}
  inline DP d35() {NP p; p.pathway = true; p.bpm = 120; p.range_down = 0; p.incomplete = true; return p;}
  //
  inline DP d40() {NP p; p.range_down = 0; p.degrees = {0,1,2,3}; return p;}
  inline DP d41() {NP p; p.range_down = 0; p.degrees = {4,5,6,7}; return p;}
  inline DP d45() {NP p; p.range_down = 0; return p;}
  // MIXER
  inline DP d90() {NP p; p.pathway = true; p.bpm = 120; p.range_down = 0; return p;}
  inline DP d91() {NP p; p.pathway = true; p.bpm = 120; p.range_down = 0; p.incomplete = true; return p;}
  inline DP d92() {NP p; p.range_down = 0; return p;}
  //----------------------------------
  // LEVEL 1
  //----------------------------------
  // WARMUP
  inline DP d100() {NP p; p.pathway = true; p.bpm = 120; p.range_down = 0; return p;}
  inline DP d101() {NP p; p.pathway = true; p.bpm = 120; p.range_down = 0; p.incomplete = true; return p;}
  // DRILLS
  inline DP d110() {NP p; p.use_anchor = true; p.tonic_anchor = ear::NoteParams::TonicAnchor::After; return p;}
  inline DP d111() {NP p; p.use_anchor = true; p.tonic_anchor = ear::NoteParams::TonicAnchor::Before; return p;}
  inline DP d115() {NP p; p.use_anchor = true; return p;}

  inline DP d125() {NP p; p.pathway = true; p.bpm = 120; p.range_down = 0; p.use_anchor = true; return p;}
  inline DP d126() {NP p; p.range_down = 0; return p;}

  inline DP d130() {NP p; p.pathway = true; p.bpm = 120; return p;}
  inline DP d131() {NP p; p.pathway = true; p.bpm = 120; p.incomplete = true; return p;}
  inline DP d135() {NP p; return p;}

  inline DP d140() {MP p; p.length = {2}; p.start={0,1,2,3}; p.max_step = 1; p.bpm = 60; p.range_down = 0; return p;} //1-4
  inline DP d141() {MP p; p.length = {2}; p.start={5,6,7,8}; p.max_step = 1; p.bpm = 60; p.range_down = 0; return p;} //5-8
  inline DP d145() {MP p; p.length = {2}; p.max_step = 1; p.bpm = 60; p.range_down = 0; return p;} //1-8

  inline DP d150() {MP p; p.length = {2}; p.start={0,1,2,3}; p.max_step = 2; p.bpm = 60; p.range_down = 0; return p;} //1-4
  inline DP d151() {MP p; p.length = {2}; p.start={5,6,7,8}; p.max_step = 2; p.bpm = 60; p.range_down = 0; return p;} //5-8
  inline DP d155() {MP p; p.length = {2}; p.max_step = 2; p.bpm = 60; p.range_down = 0; return p;} //1-8

  inline DP d160() {MP p; p.length = {2}; p.max_step = 3; p.bpm = 60; p.range_down = 0; return p;} //1-8
  inline DP d165() {MP p; p.length = {2}; p.max_step = 4; p.bpm = 60; p.range_down = 0; return p;} //1-8

  inline DP d170() {MP p; p.length = {2}; p.max_step = 5; p.bpm = 60; p.range_down = 0; return p;} //1-8
  inline DP d175() {MP p; p.length = {2}; p.max_step = 6; p.bpm = 60; p.range_down = 0; return p;} //1-8
  // MIXER
  inline DP d195() {NP p; p.pathway = true; p.bpm = 120; return p;}
  inline DP d196() {NP p; p.pathway = true; p.incomplete = true; p.bpm = 120; return p;}
  inline DP d197() {NP p; return p;}
  inline DP d198() {MP p; p.length = {2}; p.max_step = 6; p.bpm = 60; p.range_down = 0; return p;}
  //----------------------------------
  // LEVEL 2
  //----------------------------------
  // WARMUP
  inline DP d200() {NP p; p.pathway = true; p.bpm = 120; p.range_down = 0; return p;}
  inline DP d201() {NP p; p.pathway = true; p.bpm = 120; p.range_down = 0; p.incomplete = true; return p;}
  inline DP d202() {MP p; p.length = {2}; p.max_step = 3; p.bpm = 60; p.range_down = 0; return p;}
  // DRILLS
  inline DP d210() {MP p; p.length = {2,3}; p.max_step = 1; p.bpm = 90; return p;} // 1-4
  inline DP d211() {MP p; p.length = {2,3}; p.max_step = 1; p.bpm = 90; return p;} // 5-8
  inline DP d215() {MP p; p.length = {2,3}; p.max_step = 1; p.bpm = 90; return p;} // 1-8
  inline DP d220() {MP p; p.length = {2,3}; p.max_step = 2; p.bpm = 90; return p;} // 1-4
  inline DP d221() {MP p; p.length = {2,3}; p.max_step = 2; p.bpm = 90; return p;} // 5-8
  inline DP d225() {MP p; p.length = {2,3}; p.max_step = 2; p.bpm = 90; return p;} // 1-8
  inline DP d230() {MP p; p.length = {2,3}; p.max_step = 3; p.bpm = 90; return p;} // 1-4
  inline DP d231() {MP p; p.length = {2,3}; p.max_step = 3; p.bpm = 90; return p;} // 5-8
  inline DP d235() {MP p; p.length = {2,3}; p.max_step = 3; p.bpm = 90; return p;} // 1-8

} // namespace build

// -------------------------
// The manifest
// -------------------------
using M = MetaData; // demote_to, promote_to, questions
inline const std::vector<Lesson>& manifest() {
  static const std::vector<Lesson> Manifest = {
    //-------------------------------------------------------------
    // LEVEL 0
    //-------------------------------------------------------------
    Lesson{1, "M1: PW", {
      {10, &build::d10, 10, "1–4 90bpm"},
      {11, &build::d11, 10, "5–8 90bpm"},
      {15, &build::d15, 20, "90bpm"},
      }, LessonType::Lesson, M{.demote_to=0, .promote_to=2, .mix=-1},},

    Lesson{2, "M2: PW", {
    {20, &build::d20, 10, "1–4"},
    {21, &build::d21, 10, "5–8"},
    {25, &build::d25, 20, "1-8"},
    }, LessonType::Lesson, M{.demote_to=1, .promote_to=3, .mix=-1},},

    Lesson{3, "M3: PWI", {
      {30, &build::d30, 10, "1–4"},
      {31, &build::d31, 10, "5–8"},
      {35, &build::d35, 20, "1-8"},
      }, LessonType::Lesson, M{.demote_to=2, .promote_to=4},},

    Lesson{4, "M4: NOTE", {
      {40, &build::d40, 10, "1–4"},
      {41, &build::d41, 10, "5–8"},
      {45, &build::d45, 20, "1-8"},
      }, LessonType::Lesson, M{.demote_to=3, .promote_to=9, .mix=3},},
    
    Lesson{9, "M9: MIXER", {
      {90, &build::d90, 10, "PW"},
      {91, &build::d91, 10, "PWI"},
      {92, &build::d92, 20, "NOTE"},
      }, LessonType::Mixer, M{.demote_to=1, .promote_to=10, .mix=-1},},
  
    //-------------------------------------------------------------
    // LEVEL 1
    //-------------------------------------------------------------
    Lesson{10, "M10: WU PW", {
      {100, &build::d100, 10, "WU PW"},
      {101, &build::d101, 10, "WU PWI"},
      }, LessonType::Warmup, M{.demote_to=0, .promote_to=11, .mix=-1},},
    
      Lesson{11, "M11: ANCHOR", {
      {110, &build::d110, 10, "After"},
      {111, &build::d111, 10, "Before"},
      {115, &build::d115, 20, "Both"},
      }, LessonType::Lesson, M{.demote_to=10, .promote_to=12, .mix=-1},},
    
    Lesson{12, "M12: PW + TA", {
      {125, &build::d125, 10, "PW + TA"},
      {126, &build::d126, 20, "TA"},
      }, LessonType::Lesson, M{.demote_to=11, .promote_to=13, .mix=-1},},

    Lesson{13, "M13: PW", {
      {130, &build::d130, 10, "PW"},
      {131, &build::d131, 10, "PWI"},
      {135, &build::d135, 20, "NOTE"},
      }, LessonType::Lesson, M{.demote_to=12, .promote_to=14, .mix=-1},},
    Lesson{14, "M14: MELODY STEP 1", {
      {140, &build::d140, 10, "1–4"},
      {141, &build::d141, 10, "5–8"},
      {145, &build::d145, 20, "1–8"},
      }, LessonType::Lesson, M{.demote_to=13, .promote_to=15, .mix=-1},},
    Lesson{15, "M15: MELODY STEP 2", {
      {150, &build::d150, 10, "1–4"},
      {151, &build::d151, 10, "5–8"},
      {155, &build::d155, 20, "1–8"},
      }, LessonType::Lesson, M{.demote_to=14, .promote_to=16, .mix=-1},},
    Lesson{16, "M16: MELODY STEP 3", {
      {160, &build::d160, 20, "N160: MEL Step 3"},
      {165, &build::d165, 20, "N165: MEL Step 4"},
      }, LessonType::Lesson, M{.demote_to=15, .promote_to=17, .mix=-1},},
    Lesson{17, "M17: MELODY STEP 4-6", {
      {170, &build::d170, 20, "N170: MEL Step 5"},
      {175, &build::d175, 20, "N175: MEL Step 6"},
      }, LessonType::Lesson, M{.demote_to=16, .promote_to=90, .mix=-1},},
    Lesson{19, "M19: MIXER", {
      {195, &build::d195, 15, "N195: MIXER (PW)"},
      {196, &build::d196, 15, "N196: MIXER (PWI)"},
      {197, &build::d197, 15, "N197: MIXER (NOTE)"},
      {198, &build::d198, 15, "N198: MIXER (MEL)"},
      }, LessonType::Mixer, M{.demote_to=11, .promote_to=20, .mix=-1},},
    //-------------------------------------------------------------
    // LEVEL 2
    //-------------------------------------------------------------
    Lesson{20, "M20: WU MEL", {
      {200, &build::d200, 10, "N200: WU PW"},
      {201, &build::d201, 10, "N201: WU PWI"},
      {202, &build::d202, 10, "N202: WU MEL"},
    }, LessonType::Warmup, M{.demote_to=10, .promote_to=21, .mix=-1},},
    Lesson{21, "M21: MELODY STEP 1-3", {
        {210, &build::d210, 10, "N210: MEL 1–4 Step 1"},
        {211, &build::d211, 10, "N211: MEL 5–8 Step 1"},
        {215, &build::d215, 20, "N215: MEL 1–8 Step 1"},
      }, LessonType::Lesson, M{.demote_to=20, .promote_to=22, .mix=-1},
    },
    Lesson{22, "M22: MELODY STEP 2", {
      {220, &build::d220, 10, "N220: MEL 1–4 Step 2"},
      {221, &build::d221, 10, "N221: MEL 5–8 Step 2"},
      {225, &build::d225, 20, "N225: MEL 1–8 Step 2"},
    }, LessonType::Lesson, M{.demote_to=21, .promote_to=23, .mix=-1},},
    Lesson{23, "M23: MELODY STEP 3", {
      {230, &build::d230, 10, "N230: MEL 1–4 Step 3"},
      {231, &build::d231, 10, "N231: MEL 5–8 Step 3"},
      {235, &build::d235, 20, "N235: MEL 1–8 Step 3"},
    }, LessonType::Lesson, M{.demote_to=22, .promote_to=90, .mix=-1},},
    
  };
  return Manifest;
}


} // namespace ear::builtin::MelodyLevels
