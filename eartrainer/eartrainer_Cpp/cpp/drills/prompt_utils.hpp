#pragma once

#include "../include/ear/types.hpp"

#include <optional>
#include <vector>

namespace ear::drills {

inline void append_note(ear::PromptPlan& plan, int pitch, int duration_ms,
                        std::optional<int> velocity = std::nullopt,
                        std::optional<bool> tie = std::nullopt) {
  plan.notes.push_back({pitch, duration_ms, velocity, tie});
}

inline void append_rest(ear::PromptPlan& plan, int duration_ms) {
  if (duration_ms <= 0) {
    return;
  }
  constexpr int kRestPitch = -1;
  constexpr int kRestVelocity = 0;
  plan.notes.push_back({kRestPitch, duration_ms, kRestVelocity, std::nullopt});
}

} // namespace ear::drills
