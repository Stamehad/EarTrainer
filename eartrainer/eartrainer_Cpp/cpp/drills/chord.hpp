#pragma once

#include "drill.hpp"

namespace ear {

class ChordDrill : public DrillModule {
public:
  void configure(const SessionSpec& spec) override;
  DrillOutput next_question(const SessionSpec& spec, std::uint64_t& rng_state) override;

private:
  std::optional<int> last_degree_;
  std::optional<int> last_voicing_;
};

} // namespace ear
