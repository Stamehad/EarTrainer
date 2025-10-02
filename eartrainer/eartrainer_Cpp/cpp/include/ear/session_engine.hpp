#pragma once

#include "types.hpp"

#include <memory>
#include <string>
#include <variant>

namespace ear {

class SessionEngine {
public:
  virtual ~SessionEngine() = default;

  virtual std::string create_session(const SessionSpec& spec) = 0;

  using Next = std::variant<QuestionBundle, SessionSummary>;

  virtual Next next_question(const std::string& session_id) = 0;

  virtual AssistBundle assist(const std::string& session_id,
                              const std::string& question_id,
                              const std::string& kind) = 0;

  virtual Next submit_result(const std::string& session_id,
                             const ResultReport& report) = 0;

  virtual nlohmann::json debug_state(const std::string& session_id) = 0;

  virtual nlohmann::json capabilities() const = 0;
};

std::unique_ptr<SessionEngine> make_engine();

} // namespace ear
