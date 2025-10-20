#include "EarTrainerBridge.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>

#include "../Engine/include/nlohmann/json.hpp"
#include "../Engine/include/ear/session_engine.hpp"
#include "../Engine/src/json_bridge.hpp"
#include "../Engine/include/ear/types.hpp"

namespace {

struct EngineState {
  std::mutex mutex;
  std::unique_ptr<ear::SessionEngine> engine;
  std::optional<std::string> session_id;
  std::optional<ear::SessionSpec> session_spec;
  std::optional<ear::QuestionBundle> last_question;
  std::string storage_root;
  nlohmann::json profile_json = nlohmann::json::object();
  bool profile_loaded = false;
  bool session_active = false;
  std::size_t questions_answered = 0;
  std::optional<std::vector<ear::LevelCatalogEntry>> level_catalog_cache;
};

EngineState& state() {
  static EngineState instance;
  return instance;
}

ear::SessionEngine& ensure_engine() {
  auto& s = state();
  if (!s.engine) {
    s.engine = ear::make_engine();
    s.level_catalog_cache.reset();
  }
  return *s.engine;
}

char* copy_string(const std::string& value) {
  char* buffer = static_cast<char*>(std::malloc(value.size() + 1));
  if (!buffer) {
    return nullptr;
  }
  std::memcpy(buffer, value.c_str(), value.size());
  buffer[value.size()] = '\0';
  return buffer;
}

char* copy_json(const nlohmann::json& json) {
  return copy_string(json.dump());
}

nlohmann::json ok_envelope() {
  nlohmann::json payload = nlohmann::json::object();
  payload["status"] = "ok";
  return payload;
}

nlohmann::json error_envelope(const std::string& message) {
  nlohmann::json payload = nlohmann::json::object();
  payload["status"] = "error";
  payload["message"] = message;
  return payload;
}

nlohmann::json make_question_payload(const ear::QuestionBundle& bundle,
                                     const nlohmann::json& debug) {
  nlohmann::json payload = ok_envelope();
  payload["type"] = "question";
  payload["question"] = ear::bridge::to_json(bundle);
  payload["debug"] = debug;
  return payload;
}

nlohmann::json make_summary_payload(const ear::SessionSummary& summary) {
  nlohmann::json payload = ok_envelope();
  payload["type"] = "summary";
  payload["summary"] = ear::bridge::to_json(summary);
  return payload;
}

nlohmann::json make_memory_payload(const ear::MemoryPackage& package) {
  nlohmann::json payload = ok_envelope();
  payload["type"] = "summary";
  payload["summary"] = ear::bridge::to_json(package.summary);
  payload["memory"] = ear::bridge::to_json(package);
  return payload;
}

} // namespace

extern "C" {

char* set_storage_root(const char* path) {
  auto& s = state();
  std::scoped_lock guard(s.mutex);
  s.storage_root = path ? std::string(path) : std::string();
  if (!s.storage_root.empty()) {
    std::error_code ec;
    std::filesystem::create_directories(s.storage_root, ec);
    std::filesystem::current_path(s.storage_root, ec);
  }
  return nullptr;
}

char* load_profile(const char* name) {
  auto& s = state();
  std::scoped_lock guard(s.mutex);
  nlohmann::json profile = nlohmann::json::object();
  profile["name"] = name ? std::string(name) : "Player";
  profile["totalSessions"] = 0;
  profile["updatedAt"] = "1970-01-01T00:00:00Z";
  nlohmann::json settings = nlohmann::json::object();
  settings["showLatency"] = false;
  settings["enableAssistance"] = false;
  settings["useCBOR"] = false;
  profile["settings"] = settings;
  s.profile_json = profile;
  s.profile_loaded = true;
  return copy_json(profile);
}

char* serialize_profile(void) {
  auto& s = state();
  std::scoped_lock guard(s.mutex);
  if (!s.profile_loaded) {
    return load_profile("Player");
  }
  return copy_json(s.profile_json);
}

char* deserialize_profile(const char* json) {
  auto& s = state();
  std::scoped_lock guard(s.mutex);
  if (!json) {
    return copy_string("Missing profile json");
  }
  try {
    s.profile_json = nlohmann::json::parse(json);
    s.profile_loaded = true;
    return nullptr;
  } catch (const std::exception& ex) {
    return copy_string(ex.what());
  }
}

char* serialize_checkpoint(void) {
  // Checkpoint persistence is currently not supported; return null so callers know
  // there is nothing to save.
  return nullptr;
}

char* deserialize_checkpoint(const char* /*json*/) {
  // Not supported yet; report gracefully.
  return copy_string("Checkpoint restore not implemented");
}

int has_active_session(void) {
  auto& s = state();
  std::scoped_lock guard(s.mutex);
  return s.session_active ? 1 : 0;
}

char* start_session(const char* spec_json) {
  if (!spec_json) {
    return copy_string("Missing session spec json");
  }
  try {
    auto& s = state();
    std::scoped_lock guard(s.mutex);
    auto json_spec = nlohmann::json::parse(spec_json);
    ear::SessionSpec spec = ear::bridge::session_spec_from_json(json_spec);
    auto& engine = ensure_engine();
    std::string session_id = engine.create_session(spec);
    s.session_id = session_id;
    s.session_spec = spec;
    s.last_question.reset();
    s.session_active = true;
    s.questions_answered = 0;
    return nullptr;
  } catch (const std::exception& ex) {
    return copy_string(ex.what());
  }
}

char* next_question(void) {
  auto& s = state();
  std::scoped_lock guard(s.mutex);
  if (!s.session_active || !s.session_id.has_value()) {
    return copy_json(ok_envelope());
  }
  try {
    auto& engine = ensure_engine();
    auto next = engine.next_question(*s.session_id);
    if (auto bundle = std::get_if<ear::QuestionBundle>(&next)) {
      s.last_question = *bundle;
      auto debug = engine.debug_state(*s.session_id);
      return copy_json(make_question_payload(*bundle, debug));
    }
    s.session_active = false;
    auto summary = std::get<ear::SessionSummary>(next);
    return copy_json(make_summary_payload(summary));
  } catch (const std::exception& ex) {
    return copy_json(error_envelope(ex.what()));
  }
}

char* feedback(const char* answer_json) {
  auto& s = state();
  std::scoped_lock guard(s.mutex);
  if (!s.session_active || !s.session_id.has_value()) {
    return copy_json(error_envelope("No active session"));
  }
  if (!s.last_question.has_value()) {
    return copy_json(error_envelope("No pending question"));
  }
  try {
    nlohmann::json json_report;
    if (answer_json) {
      json_report = nlohmann::json::parse(answer_json);
    } else {
      json_report = nlohmann::json::object();
    }
    ear::ResultReport report = ear::bridge::result_report_from_json(json_report);
    auto& engine = ensure_engine();
    auto next = engine.submit_result(*s.session_id, report);
    s.questions_answered += 1;
    if (auto bundle = std::get_if<ear::QuestionBundle>(&next)) {
      s.last_question = *bundle;
      auto debug = engine.debug_state(*s.session_id);
      return copy_json(make_question_payload(*bundle, debug));
    }
    s.session_active = false;
    auto summary = std::get<ear::SessionSummary>(next);
    return copy_json(make_summary_payload(summary));
  } catch (const std::exception& ex) {
    return copy_json(error_envelope(ex.what()));
  }
}

char* end_session(void) {
  auto& s = state();
  std::scoped_lock guard(s.mutex);
  if (!s.session_id.has_value()) {
    return copy_json(ok_envelope());
  }
  try {
    auto& engine = ensure_engine();
    auto package = engine.end_session(*s.session_id);
    s.session_active = false;
    s.session_id.reset();
    s.last_question.reset();
    return copy_json(make_memory_payload(package));
  } catch (const std::exception& ex) {
    return copy_json(error_envelope(ex.what()));
  }
}

char* level_catalog_entries(const char* spec_json) {
  if (!spec_json) {
    return copy_json(error_envelope("Missing session spec json"));
  }
  auto& s = state();
  std::scoped_lock guard(s.mutex);
  try {
    auto json_spec = nlohmann::json::parse(spec_json);
    ear::SessionSpec spec = ear::bridge::session_spec_from_json(json_spec);
    auto& engine = ensure_engine();
    if (!s.level_catalog_cache.has_value()) {
      s.level_catalog_cache = engine.level_catalog_entries(spec);
    }
    nlohmann::json payload = ok_envelope();
    nlohmann::json entries = nlohmann::json::array();
    if (s.level_catalog_cache.has_value()) {
      for (const auto& entry : s.level_catalog_cache.value()) {
        entries.push_back(ear::bridge::to_json(entry));
      }
    }
    payload["entries"] = std::move(entries);
    return copy_json(payload);
  } catch (const std::exception& ex) {
    return copy_json(error_envelope(ex.what()));
  }
}

void free_string(char* ptr) {
  if (ptr != nullptr) {
    std::free(ptr);
  }
}

} // extern "C"
