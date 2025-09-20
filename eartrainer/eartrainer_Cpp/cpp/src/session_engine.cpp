#include "ear/session_engine.hpp"

#include "../assistance/assistance.hpp"
#include "../drills/chord.hpp"
#include "../drills/interval.hpp"
#include "../drills/melody.hpp"
#include "../drills/note.hpp"
#include "../drills/drill.hpp"
#include "../scoring/scoring.hpp"
#include "rng.hpp"

#include <algorithm>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace ear {
namespace {

std::string make_question_id(std::size_t index) {
  std::ostringstream oss;
  oss << "q-";
  oss.width(3);
  oss.fill('0');
  oss << index + 1;
  return oss.str();
}

std::unique_ptr<Sampler> make_sampler(const std::string& kind) {
  if (kind == "note") {
    return std::make_unique<NoteSampler>();
  }
  if (kind == "interval") {
    return std::make_unique<IntervalSampler>();
  }
  if (kind == "melody") {
    return std::make_unique<MelodySampler>();
  }
  if (kind == "chord" || kind == "chord_melody") {
    return std::make_unique<ChordSampler>();
  }
  throw std::runtime_error("Unsupported drill kind: " + kind);
}

std::unique_ptr<DrillModule> make_drill(const std::string& kind) {
  if (kind == "note") {
    return std::make_unique<NoteDrill>();
  }
  if (kind == "interval") {
    return std::make_unique<IntervalDrill>();
  }
  if (kind == "melody") {
    return std::make_unique<MelodyDrill>();
  }
  if (kind == "chord" || kind == "chord_melody") {
    return std::make_unique<ChordDrill>();
  }
  throw std::runtime_error("Unsupported drill kind: " + kind);
}

struct QuestionState {
  std::string id;
  std::optional<AbstractSample> sample;
  std::optional<DrillModule::DrillOutput> output;
  bool served = false;
  bool answered = false;
};

struct SubmitCache {
  ResultReport report;
  bool is_summary = false;
  std::optional<QuestionBundle> question;
  std::optional<SessionSummary> summary;
};

struct SessionData {
  SessionSpec spec;
  std::unique_ptr<Sampler> sampler;
  std::unique_ptr<DrillModule> drill;
  std::uint64_t rng_state = 0;
  bool eager_materialised = false;
  std::vector<QuestionState> questions;
  std::unordered_map<std::string, std::size_t> id_lookup;
  std::unordered_map<std::string, SubmitCache> submit_cache;
  std::vector<ResultReport> result_log;
  std::optional<std::size_t> active_question;
  SessionSummary summary_cache;
  bool summary_ready = false;
};

QuestionBundle make_bundle(SessionData& session, QuestionState& state) {
  if (!state.output.has_value()) {
    throw std::runtime_error("Question output missing");
  }
  QuestionBundle bundle;
  bundle.question_id = state.id;
  bundle.question = state.output->question;
  bundle.correct_answer = state.output->correct_answer;
  bundle.prompt = state.output->prompt;
  bundle.ui_hints = state.output->ui_hints;
  return bundle;
}

void ensure_question(SessionData& session, std::size_t index) {
  if (index >= session.questions.size()) {
    throw std::out_of_range("question index out of range");
  }
  auto& state = session.questions[index];
  if (!state.sample.has_value()) {
    state.sample = session.sampler->next(session.spec, session.rng_state);
  }
  if (!state.output.has_value()) {
    state.output = session.drill->make_question(session.spec, *state.sample);
  }
}

void materialise_all(SessionData& session) {
  if (session.eager_materialised) {
    return;
  }
  for (std::size_t i = 0; i < session.questions.size(); ++i) {
    ensure_question(session, i);
  }
  session.eager_materialised = true;
}

SessionSummary build_summary(const std::string& session_id, const SessionSpec& spec,
                             const std::vector<ResultReport>& results) {
  SessionSummary summary;
  summary.session_id = session_id;
  nlohmann::json totals = nlohmann::json::object();
  int correct = 0;
  for (const auto& r : results) {
    if (r.correct) {
      ++correct;
    }
  }
  totals["correct"] = correct;
  totals["incorrect"] = static_cast<int>(results.size()) - correct;
  totals["avg_rt_ms"] = static_cast<int>(scoring::average_response_time(results));
  summary.totals = totals;

  nlohmann::json by_category = nlohmann::json::array();
  nlohmann::json entry = nlohmann::json::object();
  entry["label"] = spec.drill_kind;
  entry["score"] = scoring::aggregate_accuracy(results);
  by_category.push_back(entry);
  summary.by_category = by_category;

  nlohmann::json res = nlohmann::json::array();
  for (const auto& r : results) {
    nlohmann::json item = nlohmann::json::object();
    item["question_id"] = r.question_id;
    item["correct"] = r.correct;
    item["rt_ms"] = r.metrics.rt_ms;
    item["score"] = scoring::score_question(r);
    res.push_back(item);
  }
  summary.results = res;
  return summary;
}

} // namespace

class SessionEngineImpl : public SessionEngine {
public:
  std::string create_session(const SessionSpec& spec) override {
    SessionData session;
    session.spec = spec;
    session.sampler = make_sampler(spec.drill_kind);
    session.drill = make_drill(spec.drill_kind);
    session.rng_state = spec.seed == 0 ? 1 : spec.seed;
    session.summary_cache.session_id = "";
    session.summary_cache.totals = nlohmann::json::object();
    session.summary_cache.by_category = nlohmann::json::array();
    session.summary_cache.results = nlohmann::json::array();

    session.questions.reserve(spec.n_questions);
    for (int i = 0; i < spec.n_questions; ++i) {
      QuestionState state;
      state.id = make_question_id(i);
      session.id_lookup[state.id] = session.questions.size();
      session.questions.push_back(state);
    }

    std::string session_id = generate_session_id();
    session.summary_cache.session_id = session_id;
    sessions_.emplace(session_id, std::move(session));
    return session_id;
  }

  Next next_question(const std::string& session_id) override {
    auto& session = get_session(session_id);
    if (session.active_question.has_value()) {
      auto& state = session.questions[session.active_question.value()];
      return make_bundle(session, state);
    }

    if (session.result_log.size() >= session.questions.size()) {
      if (!session.summary_ready) {
        session.summary_cache = build_summary(session_id, session.spec, session.result_log);
        session.summary_ready = true;
      }
      return session.summary_cache;
    }

    if (session.spec.generation == "eager" && !session.eager_materialised) {
      materialise_all(session);
    }

    std::size_t index = session.result_log.size();
    ensure_question(session, index);
    auto& state = session.questions[index];
    state.served = true;
    session.active_question = index;
    return make_bundle(session, state);
  }

  AssistBundle assist(const std::string& session_id, const std::string& question_id,
                      const std::string& kind) override {
    auto& session = get_session(session_id);
    auto it = session.id_lookup.find(question_id);
    if (it == session.id_lookup.end()) {
      throw std::runtime_error("Unknown question id");
    }
    auto& state = session.questions[it->second];
    if (!state.served) {
      throw std::runtime_error("Question not yet served");
    }
    ensure_question(session, it->second);
    auto bundle = make_bundle(session, state);
    return assistance::make_assist(bundle, kind);
  }

  Next submit_result(const std::string& session_id, const ResultReport& report) override {
    auto& session = get_session(session_id);
    auto cache_it = session.submit_cache.find(report.question_id);
    if (cache_it != session.submit_cache.end()) {
      if (cache_it->second.is_summary) {
        return cache_it->second.summary.value();
      }
      return cache_it->second.question.value();
    }

    auto id_it = session.id_lookup.find(report.question_id);
    if (id_it == session.id_lookup.end()) {
      throw std::runtime_error("Unknown question id");
    }
    auto& state = session.questions[id_it->second];
    if (!state.served) {
      throw std::runtime_error("Cannot submit result for unserved question");
    }
    if (!state.answered) {
      session.result_log.push_back(report);
      state.answered = true;
      if (session.active_question == id_it->second) {
        session.active_question.reset();
      }
    }

    Next response;
    SubmitCache submit_cache{report};

    if (session.result_log.size() >= session.questions.size()) {
      if (!session.summary_ready) {
        session.summary_cache = build_summary(session_id, session.spec, session.result_log);
        session.summary_ready = true;
      }
      response = session.summary_cache;
      submit_cache.is_summary = true;
      submit_cache.summary = session.summary_cache;
    } else {
      ensure_question(session, id_it->second);
      auto bundle = make_bundle(session, state);
      response = bundle;
      submit_cache.is_summary = false;
      submit_cache.question = bundle;
    }

    session.submit_cache[report.question_id] = submit_cache;
    return response;
  }

  nlohmann::json capabilities() const override {
    nlohmann::json caps = nlohmann::json::object();
    caps["version"] = "v1";
    nlohmann::json drills = nlohmann::json::array();
    drills.push_back("note");
    drills.push_back("interval");
    drills.push_back("melody");
    drills.push_back("chord");
    caps["drills"] = drills;
    nlohmann::json assists = nlohmann::json::array();
    assists.push_back("Replay");
    assists.push_back("GuideTone");
    assists.push_back("TempoDown");
    assists.push_back("PathwayHint");
    caps["assists"] = assists;
    return caps;
  }

private:
  SessionData& get_session(const std::string& session_id) {
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
      throw std::runtime_error("Unknown session id");
    }
    return it->second;
  }

  std::string generate_session_id() {
    std::ostringstream oss;
    oss << "sess-" << (++session_counter_);
    return oss.str();
  }

  mutable std::unordered_map<std::string, SessionData> sessions_;
  std::uint64_t session_counter_ = 0;
};

std::unique_ptr<SessionEngine> make_engine() {
  return std::make_unique<SessionEngineImpl>();
}

} // namespace ear
