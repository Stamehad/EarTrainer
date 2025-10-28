#pragma once

namespace ear {

enum class TriadQuality {
  Major = 0,
  Minor = 1,
  Diminished = 2
};

inline std::string key_quality_to_string(TriadQuality quality){
  switch (quality) {
    case TriadQuality::Major: return "major";
    case TriadQuality::Minor: return "minor";
    case TriadQuality::Diminished: return "dim";
  }
  return "major";
}


} // namespace ear

