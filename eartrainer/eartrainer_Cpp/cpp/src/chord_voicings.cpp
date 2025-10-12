#include "../include/ear/chord_voicings.hpp"

#include "../include/ear/drill_spec.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace ear {
namespace {

std::string normalize_quality(std::string quality) {
  std::transform(quality.begin(), quality.end(), quality.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return quality;
}

std::vector<int> parse_int_list(const std::string& line) {
  std::vector<int> values;
  auto start = line.find('[');
  auto end = line.find(']');
  if (start == std::string::npos || end == std::string::npos || end <= start) {
    return values;
  }
  std::string content = line.substr(start + 1, end - start - 1);
  std::stringstream ss(content);
  std::string token;
  while (std::getline(ss, token, ',')) {
    try {
      values.push_back(std::stoi(token));
    } catch (const std::exception&) {
      // ignore tokens that are not integers
    }
  }
  return values;
}

void merge_quality_entry(const std::string& quality_raw, const std::vector<int>& bass_offsets,
                         const std::vector<std::vector<int>>& voicings,
                         ChordVoicingLibrary& library) {
  std::string quality = normalize_quality(quality_raw);

  if (!bass_offsets.empty()) {
    library.bass_offsets[quality] = bass_offsets;
  }
  if (!voicings.empty()) {
    library.right_hand[quality] = voicings;
  }
}

bool load_chord_voicings_from_json(const std::filesystem::path& path,
                                   ChordVoicingLibrary& library) {
  std::ifstream in(path);
  if (!in.is_open()) {
    return false;
  }

  std::string line;
  std::string current_quality;
  std::vector<int> current_bass;
  std::vector<std::vector<int>> current_voicings;
  bool in_triads = false;
  int brace_depth = 0;

  auto flush_quality = [&](bool force) {
    if (!current_quality.empty() && (!current_bass.empty() || !current_voicings.empty())) {
      merge_quality_entry(current_quality, current_bass, current_voicings, library);
    }
    if (force) {
      current_quality.clear();
      current_bass.clear();
      current_voicings.clear();
    }
  };

  while (std::getline(in, line)) {
    std::string trimmed = line;
    trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n"));
    if (trimmed.empty()) {
      continue;
    }

    int open_count = static_cast<int>(std::count(trimmed.begin(), trimmed.end(), '{'));
    int close_count = static_cast<int>(std::count(trimmed.begin(), trimmed.end(), '}'));
    int depth_before = brace_depth;

    if (!in_triads) {
      if (trimmed.find("\"triads\"") != std::string::npos) {
        in_triads = true;
      }
      brace_depth += open_count;
      brace_depth -= close_count;
      continue;
    }

    if (depth_before == 1) {
      // Expecting quality key
      auto quote_pos = trimmed.find('"');
      if (quote_pos != std::string::npos) {
        auto second_quote = trimmed.find('"', quote_pos + 1);
        if (second_quote != std::string::npos) {
          current_quality = trimmed.substr(quote_pos + 1, second_quote - quote_pos - 1);
          current_bass.clear();
          current_voicings.clear();
        }
      }
      continue;
    }

    if (depth_before >= 2) {
      if (trimmed.find("\"bass_offsets\"") != std::string::npos) {
        current_bass = parse_int_list(trimmed);
      } else if (trimmed.find("\"offsets\"") != std::string::npos) {
        auto offsets = parse_int_list(trimmed);
        if (!offsets.empty()) {
          current_voicings.push_back(std::move(offsets));
        }
      }
    }

    brace_depth += open_count;
    brace_depth -= close_count;

    if (close_count > 0) {
      if (brace_depth == 1 && depth_before >= 2) {
        flush_quality(true);
      }
      if (brace_depth <= 0) {
        break;
      }
    }
  }

  flush_quality(false);
  return !library.right_hand.empty();
}

std::vector<std::string> candidate_relative_paths(const DrillSpec& spec) {
  std::vector<std::string> candidates;
  if (spec.params.is_object() && spec.params.contains("voicings_file") &&
      spec.params["voicings_file"].is_string()) {
    candidates.push_back(spec.params["voicings_file"].get<std::string>());
  }
  candidates.emplace_back("voicings/piano_voicing.json");
  candidates.emplace_back("resources/voicings/piano_voicing.json");
  candidates.emplace_back("eartrainer/eartrainer_Cpp/resources/voicings/piano_voicing.json");
  return candidates;
}

std::vector<std::filesystem::path> candidate_roots(const DrillSpec& spec) {
  std::vector<std::filesystem::path> roots;
  if (spec.params.is_object() && spec.params.contains("resources_dir") &&
      spec.params["resources_dir"].is_string()) {
    roots.emplace_back(spec.params["resources_dir"].get<std::string>());
  }
  roots.emplace_back();
  roots.emplace_back("eartrainer/eartrainer_Cpp");
  roots.emplace_back("eartrainer/eartrainer_Cpp/resources");
  roots.emplace_back("resources");
  return roots;
}

} // namespace

ChordVoicingLibrary default_chord_voicings() {
  ChordVoicingLibrary library;
  library.bass_offsets = {
      {"major", {-14}},
      {"minor", {-14}},
      {"diminished", {-14}},
  };
  library.right_hand = {
      {"major", {{0, 2, 4, 7}, {0, 2, 4}, {-3, 0, 2}}},
      {"minor", {{0, 2, 4, 7}, {0, 2, 4}, {-3, 0, 2}}},
      {"diminished", {{0, 2, 4, 7}, {0, 2, 4}, {-3, 0, 2}}},
  };
  return library;
}

std::optional<std::filesystem::path> configure_chord_voicings(const DrillSpec& spec,
                                                              ChordVoicingLibrary& library) {
  auto rel_candidates = candidate_relative_paths(spec);
  auto roots = candidate_roots(spec);

  for (const auto& candidate : rel_candidates) {
    if (candidate.empty()) {
      continue;
    }
    std::filesystem::path path(candidate);
    if (path.is_absolute()) {
      if (load_chord_voicings_from_json(path, library)) {
        return path;
      }
      continue;
    }
    if (load_chord_voicings_from_json(path, library)) {
      return std::filesystem::absolute(path);
    }
    for (const auto& root : roots) {
      std::filesystem::path resolved = root.empty() ? path : root / path;
      if (load_chord_voicings_from_json(resolved, library)) {
        return std::filesystem::absolute(resolved);
      }
    }
  }

  return std::nullopt;
}

} // namespace ear
