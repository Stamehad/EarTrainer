#pragma once

#include "drill.hpp"

#include <optional>
#include <string>
#include <vector>

namespace ear {

struct HarmonyClusterPattern {
  int cluster_id = 0;
  std::string id;
  std::vector<int> relative_degrees;
};

class HarmonyDrill : public DrillModule {
public:
  void configure(const DrillSpec& spec) override;
  QuestionBundle next_question(std::uint64_t& rng_state) override;

private:
  DrillSpec spec_{};
  IntervalParams params{};
  int tonic_midi_ = 60;
  bool avoid_repeat_ = true;
  std::vector<int> allowed_root_degrees_;
  std::vector<HarmonyClusterPattern> patterns_;
  std::optional<int> last_root_degree_;
  std::optional<std::size_t> last_pattern_index_;
};

} // namespace ear
