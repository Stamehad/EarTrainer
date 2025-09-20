#pragma once

#include "drill.hpp"

#include <deque>
#include <vector>

namespace ear {

class MelodySampler : public Sampler {
public:
  AbstractSample next(const SessionSpec& spec, std::uint64_t& rng_state) override;

private:
  std::deque<std::vector<int>> recent_sequences_;
  static constexpr std::size_t kRecentCapacity = 16;
};

class MelodyDrill : public DrillModule {
public:
  DrillOutput make_question(const SessionSpec& spec, const AbstractSample& sample) override;
};

} // namespace ear
