#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace ear::pathways {

struct PathwayPattern {
  std::vector<int> degrees;
};

struct PathwayOptions {
  PathwayPattern primary;
  std::vector<PathwayPattern> alternatives;
};

using DegreePathways = std::unordered_map<int, PathwayOptions>;
using PathwayBank = std::unordered_map<std::string, DegreePathways>;

const PathwayBank& default_bank();

std::string infer_scale_type(const std::string& key);

const PathwayOptions* find_pathway(const PathwayBank& bank, const std::string& scale_type,
                                   int degree);

} // namespace ear::pathways

