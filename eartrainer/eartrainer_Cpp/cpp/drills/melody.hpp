#pragma once

#include "drill.hpp"

#include <deque>
#include <vector>

namespace ear {

class MelodyDrill : public DrillModule {
public:
  void configure(const DrillSpec& spec) override;
  QuestionBundle next_question(std::uint64_t& rng_state) override;

private:
  DrillSpec spec_{};
  MelodyParams params{};
  std::deque<std::vector<int>> recent_sequences_;
  int tonic_midi;
  std::pair<int,int> midi_range;
  static constexpr std::size_t kRecentCapacity = 16;
};

} // namespace ear
