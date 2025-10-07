#pragma once

#include "drill.hpp"

namespace ear {

class ChordDrill : public DrillModule {
public:
  void configure(const DrillSpec& spec) override;
  DrillOutput next_question(std::uint64_t& rng_state) override;

private:
  DrillSpec spec_{};
  std::optional<int> last_degree_;
  std::optional<int> last_voicing_;
};

} // namespace ear
