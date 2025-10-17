#pragma once

#include "drill.hpp"

#include <optional>

namespace ear {

class NoteDrill : public DrillModule {
public:
  void configure(const DrillSpec& spec) override;
  QuestionBundle next_question(std::uint64_t& rng_state) override;

private:
  DrillSpec spec_{};
  std::optional<int> last_degree_;
  std::optional<int> last_midi_;
};

} // namespace ear
