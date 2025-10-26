#pragma once

#include "../include/ear/types.hpp"
#include "../include/ear/drill_spec.hpp"
#include "../include/ear/question_bundle_v2.hpp"

#include <optional>
#include <utility>

namespace ear {

class DrillModule {
public:
  virtual ~DrillModule() = default;

  // Configure the drill with an immutable drill specification.
  virtual void configure(const DrillSpec& spec) = 0;

  // Produce the next question. `rng_state` is a mutable seed owned by the caller.
  virtual QuestionBundle next_question(std::uint64_t& rng_state) = 0;

  // Allow modules to observe feedback when available. Default is no-op.
  virtual void apply_feedback(const ResultReport& /*report*/) {}
};

} // namespace ear
