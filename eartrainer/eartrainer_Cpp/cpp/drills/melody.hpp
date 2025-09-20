#pragma once

#include "drill.hpp"

namespace ear {

class MelodySampler : public Sampler {
public:
  AbstractSample next(const SessionSpec& spec, std::uint64_t& rng_state) override;
};

class MelodyDrill : public DrillModule {
public:
  DrillOutput make_question(const SessionSpec& spec,
                            const AbstractSample& sample) override;
};

} // namespace ear
