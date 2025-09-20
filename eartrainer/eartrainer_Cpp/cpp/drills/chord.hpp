#pragma once

#include "drill.hpp"

namespace ear {

class ChordSampler : public Sampler {
public:
  AbstractSample next(const SessionSpec& spec, std::uint64_t& rng_state) override;

private:
  std::optional<int> last_degree_;
  std::optional<int> last_voicing_;
};

class ChordDrill : public DrillModule {
public:
  DrillOutput make_question(const SessionSpec& spec,
                            const AbstractSample& sample) override;
};

} // namespace ear
