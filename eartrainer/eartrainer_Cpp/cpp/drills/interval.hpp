#pragma once

#include "drill.hpp"

namespace ear {

class IntervalDrill : public DrillModule {
public:
  void configure(const DrillSpec& spec) override;
  QuestionBundle next_question(std::uint64_t& rng_state) override;

private:
  DrillSpec spec_{};
  std::optional<int> last_bottom_degree_;
  std::optional<int> last_bottom_midi_;
};

} // namespace ear
