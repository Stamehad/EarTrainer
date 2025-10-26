#pragma once

#include "drill.hpp"

#include <optional>

namespace ear {

class NoteDrill : public DrillModule {
public:
  void configure(const DrillSpec& spec) override;
  QuestionsBundle next_question(std::uint64_t& rng_state) override;

private:
  DrillSpec spec_{};
  NoteParams params{};
  std::pair <int, int> midi_range{};
  int tonic_midi;
  std::optional<int> last_degree_;
  std::optional<int> last_midi_;
};

} // namespace ear
