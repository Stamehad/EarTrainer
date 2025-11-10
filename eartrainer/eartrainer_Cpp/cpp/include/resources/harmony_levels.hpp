#pragma once
#include "resources/level_catalog.hpp"
#include "resources/catalog_base.hpp"

namespace ear::builtin::HarmonyLevels {

using namespace ear::builtin::catalog_numbered;

namespace build {
  using DP = ear::DrillParams;
  using HP = ear::IntervalParams;

  //----------------------------------
  // LEVEL 2
  //----------------------------------
  // P5 (TRITONE FOR 7THS)
  inline DP d2200(){HP p; p.intervals={4}; p.helper=1; return p;}
  inline DP d2201(){HP p; p.intervals={4}; p.helper=-1; return p;}
  inline DP d2205(){HP p; p.intervals={4}; p.helper=2; return p;}
  // P4 (TITONE FOR 4THS)
  inline DP d2210(){HP p; p.intervals={3}; p.helper=1; return p;}
  inline DP d2211(){HP p; p.intervals={3}; p.helper=-1; return p;}
  inline DP d2215(){HP p; p.intervals={3}; p.helper=2; return p;}
  // P4/P5
  inline DP d2220(){HP p; p.intervals={3,4}; p.helper=1; return p;}
  inline DP d2221(){HP p; p.intervals={3,4}; p.helper=-1; return p;}
  inline DP d2225(){HP p; p.intervals={3,4}; p.helper=2; return p;}
  // M3/m3
  inline DP d2230(){HP p; p.intervals={2}; p.helper=1; return p;}
  inline DP d2231(){HP p; p.intervals={2}; p.helper=-1; return p;}
  inline DP d2235(){HP p; p.intervals={2}; p.helper=2; return p;}
  // P5/P4 NO HELPER
  inline DP d2240(){HP p; p.intervals={4}; return p;}
  inline DP d2241(){HP p; p.intervals={3}; return p;}
  inline DP d2245(){HP p; p.intervals={3,4}; return p;}
  // M3/m3 NO HELPER
  inline DP d2250(){HP p; p.intervals={2}; return p;}
  inline DP d2255(){HP p; p.intervals={2,4}; return p;}
  // MIXER
  inline DP d2290(){HP p; p.intervals={2,3,4}; p.helper=2; return p;}
  inline DP d2295(){HP p; p.intervals={2,3,4}; return p;}

  //----------------------------------
  // LEVEL 3
  //----------------------------------
  // WARM UP
  inline DP d2300(){HP p; p.intervals={2,3,4}; p.helper=2; return p;} 
  inline DP d2305(){HP p; p.intervals={2,3,4}; return p;}
  // M6/m6
  inline DP d2310(){HP p; p.intervals={5}; p.helper=1; return p;}
  inline DP d2311(){HP p; p.intervals={5}; p.helper=-1; return p;}
  inline DP d2315(){HP p; p.intervals={5}; p.helper=2; return p;}
  // PIANO
  inline DP d2320(){HP p; p.intervals={2,3,4}; return p;}  
  inline DP d2325(){HP p; p.intervals={2,3,4}; p.program=0; return p;}
  // NO HELPER
  inline DP d2330(){HP p; p.intervals={2,5}; p.helper=2; return p;}
  inline DP d2335(){HP p; p.intervals={2,5}; return p;}
  // M2/m2
  inline DP d2340(){HP p; p.intervals={1}; p.helper=1; return p;}
  inline DP d2341(){HP p; p.intervals={1}; p.helper=-1; return p;}
  inline DP d2345(){HP p; p.intervals={1}; p.helper=2; return p;}
  // M7/m7
  inline DP d2350(){HP p; p.intervals={6}; p.helper=1; return p;}
  inline DP d2351(){HP p; p.intervals={6}; p.helper=-1; return p;}
  inline DP d2355(){HP p; p.intervals={6}; p.helper=2; return p;}
  // NO HELPER
  inline DP d2360(){HP p; p.intervals={1,6}; p.helper=2; return p;}
  inline DP d2365(){HP p; p.intervals={1,6}; return p;}
  // STRINGS + PIANO
  inline DP d2370(){HP p; p.intervals={1,5,6}; return p;}
  inline DP d2375(){HP p; p.intervals={1,5,6}; p.program=0; return p;}
  // MIXER
  inline DP d2390(){HP p; p.intervals={1,2,3,4,5,6}; p.helper=2; return p;}
  inline DP d2395(){HP p; p.intervals={1,2,3,4,5,6}; return p;}
  inline DP d2496(){HP p; p.intervals={1,2,3,4,5,6}; p.program=0; return p;}
  
  //----------------------------------
  // LEVEL 4
  //----------------------------------
  // WARM UP
  inline DP d2405(){HP p; p.intervals={1,2,3,4,5,6}; p.helper=2; return p;}
  // 
  inline DP d2410(){HP p; p.intervals={3,4}; return p;}
  inline DP d2411(){HP p; p.intervals={2,5}; return p;}
  inline DP d2415(){HP p; p.intervals={2,3,4,5}; return p;}
  // 
  inline DP d2420(){HP p; p.intervals={1,6}; return p;}
  inline DP d2421(){HP p; p.intervals={2,5}; return p;}
  inline DP d2425(){HP p; p.intervals={1,2,5,6}; return p;}
  // 
  inline DP d2435(){HP p; p.intervals={1,2,3,4,5,6}; return p;}
  // PIANO
  inline DP d2440(){HP p; p.intervals={3,4}; p.program=0; return p;}
  inline DP d2441(){HP p; p.intervals={2,5}; p.program=0; return p;}
  inline DP d2445(){HP p; p.intervals={2,3,4,5}; p.program=0; return p;}
  // 
  inline DP d2450(){HP p; p.intervals={1,6}; p.program=0; return p;}
  inline DP d2451(){HP p; p.intervals={2,5}; p.program=0; return p;}
  inline DP d2455(){HP p; p.intervals={1,2,5,6}; p.program=0; return p;}
  // 
  inline DP d2465(){HP p; p.intervals={1,2,3,4,5,6}; p.program=0; return p;}
  // MIXER
  inline DP d2495(){HP p; p.intervals={1,2,3,4,5,6}; return p;}
  inline DP d2497(){HP p; p.intervals={1,2,3,4,5,6}; p.program=0; return p;}

}

using M = MetaData; // demote_to, promote_to, questions
inline static const std::vector<Lesson>& manifest() {
  static const std::vector<Lesson> Manifest = {
    // LEVEL 2
    Lesson{220, "N200: P5", {
      {2200, &build::d2200, 10, "melody up"},
      {2201, &build::d2201, 10, "melody down"},
      {2205, &build::d2205, 20, "melody up/down"},
      }, LessonType::Lesson, M{.demote_to=1, .promote_to=221},
    },
    Lesson{221, "N221: P4", {
      {2210, &build::d2210, 10, "melody up"},
      {2211, &build::d2211, 10, "melody down"},
      {2215, &build::d2215, 20, "melody up/down"},
      }, LessonType::Lesson, M{.demote_to=220, .promote_to=222},
    },
    Lesson{222, "N222: P4 & P5", {
      {2220, &build::d2220, 10, "melody up"},
      {2221, &build::d2221, 10, "melody down"},
      {2225, &build::d2225, 20, "melody up/down"},
    }, LessonType::Lesson, M{.demote_to=221, .promote_to=223},},

    Lesson{223, "N223: M3 & m3", {
      {2230, &build::d2230, 10, "melody up"},
      {2231, &build::d2231, 10, "melody down"},
      {2235, &build::d2235, 20, "melody up/down"},
    }, LessonType::Lesson, M{.demote_to=222, .promote_to=224},},

    Lesson{224, "N224: P4/P5 No Helper", {
      {2240, &build::d2240, 10, "P5"},
      {2241, &build::d2241, 10, "P4"},
      {2245, &build::d2245, 20, "P4 & P5"},
    }, LessonType::Lesson, M{.demote_to=223, .promote_to=225},},

    Lesson{225, "N225: M3/m3 No Helper", {
      {2250, &build::d2250, 10, "M3/m3"},
      {2255, &build::d2255, 20, "M3/m3 & P5"},
    }, LessonType::Lesson, M{.demote_to=224, .promote_to=229},},

    Lesson{229, "N229: Interval Mixer", {
      {2290, &build::d2290, 30, "with helper"},
      {2295, &build::d2295, 30, "no helper"},
    }, LessonType::Mixer, M{.demote_to=225, .promote_to=230},},
    
  };
  return Manifest;
}

// using Catalog = NumberedCatalog<Provider>;

// // Public API (unchanged)
// inline const std::vector<int>& known_levels()                    { return Catalog::known_levels(); }
// inline const std::map<int, std::vector<int>>& phases()           { return Catalog::phases(); }
// inline const std::vector<DrillSpec>& drills_for_level(int level) { return Catalog::drills_for_level(level); }

} // namespace ear::builtin::HarmonyLevels
