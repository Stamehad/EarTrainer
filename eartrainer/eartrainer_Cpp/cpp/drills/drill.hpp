#pragma once

#include "ear/types.hpp"

#include <utility>

namespace ear {

struct AbstractSample {
  std::string kind;
  nlohmann::json degrees;
};

class Sampler {
public:
  virtual ~Sampler() = default;
  virtual AbstractSample next(const SessionSpec& spec, std::uint64_t& rng_state) = 0;
};

class DrillModule {
public:
  virtual ~DrillModule() = default;
  struct DrillOutput {
    TypedPayload question;
    TypedPayload correct_answer;
    std::optional<PromptPlan> prompt;
    nlohmann::json ui_hints;
  };

  virtual DrillOutput make_question(const SessionSpec& spec, const AbstractSample& sample) = 0;
};

} // namespace ear
