#pragma once

#include "drill.hpp"

#include <optional>

namespace ear {

class NoteSampler : public Sampler {
public:
  AbstractSample next(const SessionSpec& spec, std::uint64_t& rng_state) override;

private:
  std::optional<int> last_degree_;
};

class NoteDrill : public DrillModule {
public:
  DrillOutput make_question(const SessionSpec& spec, const AbstractSample& sample) override;
};

} // namespace ear

