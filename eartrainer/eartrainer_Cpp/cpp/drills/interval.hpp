#pragma once

#include "drill.hpp"

namespace ear {

class IntervalDrill : public DrillModule {
public:
  void configure(const SessionSpec& spec) override;
  DrillOutput next_question(const SessionSpec& spec, std::uint64_t& rng_state) override;

private:
  std::optional<int> last_bottom_degree_;
  std::optional<int> last_bottom_midi_;
};

} // namespace ear
