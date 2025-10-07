#pragma once

#include "ear/types.hpp"
#include "ear/drill_spec.hpp"

#include <optional>
#include <utility>

namespace ear {

// A generated question ready to be handed to the UI layer.
struct DrillOutput {
  TypedPayload question;
  TypedPayload correct_answer;
  std::optional<PromptPlan> prompt;
  nlohmann::json ui_hints;
};

class DrillModule {
public:
  virtual ~DrillModule() = default;

  // Configure the drill with an immutable drill specification.
  virtual void configure(const DrillSpec& spec) = 0;

  // Produce the next question. `rng_state` is a mutable seed owned by the caller.
  virtual DrillOutput next_question(std::uint64_t& rng_state) = 0;

  // Allow modules to observe feedback when available. Default is no-op.
  virtual void apply_feedback(const ResultReport& /*report*/) {}
};

} // namespace ear
