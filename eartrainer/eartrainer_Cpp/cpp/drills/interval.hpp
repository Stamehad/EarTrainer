#pragma once

#include "drill.hpp"

namespace ear {

class IntervalSampler : public Sampler {
public:
  AbstractSample next(const SessionSpec& spec, std::uint64_t& rng_state) override;

private:
  std::optional<int> last_bottom_degree_;
  std::optional<int> last_bottom_midi_;
};

class IntervalDrill : public DrillModule {
public:
  DrillOutput make_question(const SessionSpec& spec, const AbstractSample& sample) override;
};

} // namespace ear

