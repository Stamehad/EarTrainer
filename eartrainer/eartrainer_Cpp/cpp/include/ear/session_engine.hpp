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

  virtual AssistBundle session_assist(const std::string& session_id,
                                      const std::string& kind) = 0;

  virtual Next submit_result(const std::string& session_id,
                             const ResultReport& report) = 0;

  virtual MemoryPackage end_session(const std::string& session_id) = 0;

  virtual std::string session_key(const std::string& session_id) = 0;

  virtual MidiClip orientation_prompt(const std::string& session_id) = 0;

  virtual nlohmann::json debug_state(const std::string& session_id) = 0;

  virtual nlohmann::json capabilities() const = 0;

  virtual nlohmann::json adaptive_diagnostics(const std::string& session_id) = 0;

  virtual void set_level(const std::string& session_id, int level, int tier) = 0;

  virtual std::string level_catalog_overview(const std::string& session_id) = 0;

  virtual std::string level_catalog_levels(const std::string& session_id) = 0;

  virtual std::vector<LevelCatalogEntry> level_catalog_entries(const SessionSpec& spec) = 0;
};

std::unique_ptr<SessionEngine> make_engine();

} // namespace ear
