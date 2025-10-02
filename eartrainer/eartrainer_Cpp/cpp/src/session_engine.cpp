#include "ear/session_engine.hpp"

#include "../assistance/assistance.hpp"
#include "../drills/chord.hpp"
#include "../drills/interval.hpp"
#include "../drills/melody.hpp"
#include "../drills/note.hpp"
#include "../drills/drill.hpp"
#include "../drills/common.hpp"
#include "../scoring/scoring.hpp"
#include "ear/drill_hub.hpp"
#include "rng.hpp"

#include <algorithm>
#include <cctype>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

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
  std::unordered_map<std::string, PromptPlan> session_assists;

  bool adaptive = false;
  std::unique_ptr<DrillHub> drill_hub;
  double adaptive_fitness = 0.5;
  std::size_t adaptive_target_questions = 0;
  std::size_t adaptive_asked = 0;
  std::unordered_map<std::string, SessionSpec> adaptive_specs;
  std::unordered_map<std::string, std::size_t> adaptive_counts;
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

SessionSummary build_summary(const std::string& session_id, const std::string& label,
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
  entry["label"] = label;
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

std::string normalize_kind(const std::string& kind) {
  std::string normalized;
  normalized.reserve(kind.size());
  for (char ch : kind) {
    if (std::isalnum(static_cast<unsigned char>(ch))) {
      normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    } else if (ch == ' ' || ch == '-' || ch == '_') {
      normalized.push_back('_');
    }
  }
  if (normalized.empty()) {
    normalized = "unknown";
  }
  return normalized;
}

PromptPlan make_tonic_prompt(const SessionSpec& spec) {
  PromptPlan plan;
  plan.modality = "midi";
  plan.count_in = false;
  plan.tempo_bpm = spec.tempo_bpm.has_value() ? spec.tempo_bpm : std::optional<int>(96);

  const int tonic = drills::central_tonic_midi(spec.key);
  const int min_pitch = std::max(0, spec.range_min);
  const int max_pitch = std::min(127, spec.range_max > 0 ? spec.range_max : 127);
  const int clamped = std::max(min_pitch, std::min(max_pitch, tonic));

  plan.notes.push_back({clamped, 900, std::nullopt, std::nullopt});
  return plan;
}

PromptPlan make_scale_arpeggio_prompt(const SessionSpec& spec) {
  PromptPlan plan;
  plan.modality = "midi";
  plan.count_in = false;
  plan.tempo_bpm = spec.tempo_bpm.has_value() ? spec.tempo_bpm : std::optional<int>(96);

  const std::vector<int> pattern = {0, 1, 2, 3, 4, 5, 6, 7, 4, 2, 0};
  const int min_pitch = std::max(0, spec.range_min);
  const int max_pitch = std::min(127, spec.range_max > 0 ? spec.range_max : 127);

  for (std::size_t i = 0; i < pattern.size(); ++i) {
    int degree = pattern[i];
    int midi = drills::degree_to_midi(spec, degree);
    midi = std::max(min_pitch, std::min(max_pitch, midi));
    int dur = (i == pattern.size() - 1) ? 520 : 320;
    plan.notes.push_back({midi, dur, std::nullopt, std::nullopt});
  }
  return plan;
}

std::unordered_map<std::string, PromptPlan> build_session_assists(const SessionSpec& spec) {
  std::unordered_map<std::string, PromptPlan> assists;
  assists.emplace(normalize_kind("ScaleArpeggio"), make_scale_arpeggio_prompt(spec));
  assists.emplace(normalize_kind("Tonic"), make_tonic_prompt(spec));
  return assists;
}

AssistBundle session_assist_bundle(SessionData& session, const std::string& kind,
                                   const std::string& question_id) {
  AssistBundle bundle;
  bundle.question_id = question_id;
  bundle.kind = kind;
  bundle.prompt = std::nullopt;
  bundle.ui_delta = nlohmann::json::object();

  std::string normalized = normalize_kind(kind);
  auto it = session.session_assists.find(normalized);
  if (it != session.session_assists.end()) {
    bundle.prompt = it->second;
    bundle.ui_delta["message"] = kind;
  } else {
    bundle.ui_delta["message"] = "Assist not available";
  }
  return bundle;
}


} // namespace

class SessionEngineImpl : public SessionEngine {
public:
  std::string create_session(const SessionSpec& spec) override {
    if (spec.adaptive) {
      return create_adaptive_session(spec);
    }

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
    session.session_assists = build_session_assists(session.spec);
    sessions_.emplace(session_id, std::move(session));
    return session_id;
  }

  Next next_question(const std::string& session_id) override {
    auto& session = get_session(session_id);
    if (session.adaptive) {
      return next_question_adaptive(session_id, session);
    }
    if (session.active_question.has_value()) {
      auto& state = session.questions[session.active_question.value()];
      return make_bundle(session, state);
    }

    if (session.result_log.size() >= session.questions.size()) {
      if (!session.summary_ready) {
        session.summary_cache =
            build_summary(session_id, session.spec.drill_kind, session.result_log);
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
    auto normalized_kind = normalize_kind(kind);
    if (session.session_assists.find(normalized_kind) != session.session_assists.end()) {
      if (!question_id.empty()) {
        auto id_check = session.id_lookup.find(question_id);
        if (id_check == session.id_lookup.end()) {
          throw std::runtime_error("Unknown question id");
        }
      }
      return session_assist_bundle(session, kind, question_id);
    }
    if (session.adaptive) {
      auto it = session.id_lookup.find(question_id);
      if (it == session.id_lookup.end()) {
        throw std::runtime_error("Unknown question id");
      }
      auto& state = session.questions[it->second];
      if (!state.output.has_value()) {
        throw std::runtime_error("Question not yet materialised");
      }
      auto bundle = make_bundle(session, state);
      return assistance::make_assist(bundle, kind);
    }
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

  AssistBundle session_assist(const std::string& session_id,
                              const std::string& kind) override {
    auto& session = get_session(session_id);
    return session_assist_bundle(session, kind, "");
  }

  Next submit_result(const std::string& session_id, const ResultReport& report) override {
    auto& session = get_session(session_id);
    if (session.adaptive) {
      return submit_result_adaptive(session_id, session, report);
    }
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
        session.summary_cache =
            build_summary(session_id, session.spec.drill_kind, session.result_log);
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
    nlohmann::json session_assists = nlohmann::json::array();
    session_assists.push_back("ScaleArpeggio");
    session_assists.push_back("Tonic");
    caps["session_assists"] = session_assists;
    return caps;
  }

  nlohmann::json debug_state(const std::string& session_id) override {
    auto& session = get_session(session_id);
    nlohmann::json info = nlohmann::json::object();
    info["session_id"] = session_id;
    info["mode"] = session.adaptive ? "adaptive" : "manual";
    info["summary_ready"] = session.summary_ready;
    info["result_count"] = static_cast<int>(session.result_log.size());

    if (session.adaptive) {
      info["adaptive_fitness"] = session.adaptive_fitness;
      info["adaptive_asked"] = static_cast<int>(session.adaptive_asked);
      info["adaptive_target"] = static_cast<int>(session.adaptive_target_questions);
      nlohmann::json counts = nlohmann::json::object();
      for (const auto& kv : session.adaptive_counts) {
        counts[kv.first] = static_cast<int>(kv.second);
      }
      info["drill_counts"] = counts;
      if (session.drill_hub) {
        info["drill_hub"] = session.drill_hub->debug_state();
      }
    } else {
      info["drill_kind"] = session.spec.drill_kind;
      info["total_questions"] = static_cast<int>(session.questions.size());
      info["eager_materialised"] = session.eager_materialised;
    }

    return info;
  }

private:
  std::string create_adaptive_session(const SessionSpec& spec);
  Next next_question_adaptive(const std::string& session_id, SessionData& session);
  Next submit_result_adaptive(const std::string& session_id, SessionData& session,
                              const ResultReport& report);

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

std::string SessionEngineImpl::create_adaptive_session(const SessionSpec& spec) {
  SessionData session;
  session.spec = spec;
  session.spec.drill_kind = "adaptive";
  session.adaptive = true;
  session.summary_cache.session_id = "";
  session.summary_cache.totals = nlohmann::json::object();
  session.summary_cache.by_category = nlohmann::json::array();
  session.summary_cache.results = nlohmann::json::array();

  session.adaptive_target_questions = spec.n_questions > 0 ? static_cast<std::size_t>(spec.n_questions)
                                                          : static_cast<std::size_t>(0);

  if (spec.sampler_params.is_object() && spec.sampler_params.contains("initial_fitness")) {
    const auto& value = spec.sampler_params["initial_fitness"];
    if (value.is_number()) {
      session.adaptive_fitness = value.get<double>();
    }
  }

  std::vector<std::string> drill_kinds;
  if (spec.sampler_params.is_object() && spec.sampler_params.contains("drills")) {
    const auto& drills_json = spec.sampler_params["drills"];
    if (drills_json.is_array()) {
      const auto& arr = drills_json.get_array();
      for (const auto& value : arr) {
        if (value.is_string()) {
          drill_kinds.push_back(value.get<std::string>());
        }
      }
    }
  }
  if (drill_kinds.empty()) {
    if (!spec.drill_kind.empty() && spec.drill_kind != "adaptive") {
      drill_kinds.push_back(spec.drill_kind);
    } else {
      drill_kinds.push_back("melody");
    }
  }

  std::vector<DrillHub::Entry> entries;
  entries.reserve(drill_kinds.size());
  for (const auto& kind : drill_kinds) {
    DrillHub::Entry entry;
    entry.drill_kind = kind;
    entry.sampler = make_sampler(kind);
    entry.drill = make_drill(kind);
    SessionSpec drill_spec = spec;
    drill_spec.drill_kind = kind;
    drill_spec.adaptive = false;
    drill_spec.n_questions = 1;
    entry.spec = std::move(drill_spec);
    entry.weight = 1.0;
    entries.push_back(std::move(entry));
  }

  if (entries.empty()) {
    throw std::runtime_error("Adaptive session requires at least one drill kind");
  }

  session.drill_hub =
      std::make_unique<DrillHub>(std::move(entries), spec.seed == 0 ? 1 : spec.seed);

  if (spec.sampler_params.is_object() && session.drill_hub &&
      spec.sampler_params.contains("weights")) {
    const auto& weights_json = spec.sampler_params["weights"];
    if (weights_json.is_object()) {
      std::unordered_map<std::string, double> bias;
      const auto& obj = weights_json.get_object();
      for (const auto& kv : obj) {
        if (kv.second.is_number()) {
          bias[kv.first] = kv.second.get<double>();
        }
      }
      if (!bias.empty()) {
        session.drill_hub->set_weight_bias(bias);
      }
    }
  }

  std::string session_id = generate_session_id();
  session.summary_cache.session_id = session_id;
  session.session_assists = build_session_assists(session.spec);
  sessions_.emplace(session_id, std::move(session));
  return session_id;
}

SessionEngine::Next SessionEngineImpl::next_question_adaptive(const std::string& session_id,
                                                              SessionData& session) {
  if (session.summary_ready &&
      session.result_log.size() >= session.adaptive_target_questions &&
      session.adaptive_target_questions != 0) {
    return session.summary_cache;
  }

  if (session.active_question.has_value()) {
    auto& state = session.questions[session.active_question.value()];
    return make_bundle(session, state);
  }

  if (!session.drill_hub) {
    throw std::runtime_error("Adaptive session missing drill hub");
  }

  if (session.adaptive_target_questions != 0 &&
      session.adaptive_asked >= session.adaptive_target_questions) {
    if (!session.summary_ready) {
      session.summary_cache =
          build_summary(session_id, session.spec.drill_kind, session.result_log);
      session.summary_ready = true;
    }
    return session.summary_cache;
  }

  auto selection = session.drill_hub->next();
  std::size_t index = session.questions.size();
  std::string question_id = make_question_id(index);

  QuestionState state;
  state.id = question_id;
  state.sample = std::move(selection.sample);
  state.output = std::move(selection.output);
  state.served = true;
  state.answered = false;

  session.id_lookup[question_id] = index;
  session.questions.push_back(std::move(state));
  session.adaptive_specs[question_id] = std::move(selection.spec);
  session.active_question = index;
  session.adaptive_asked += 1;
  session.adaptive_counts[selection.drill_kind] += 1;

  auto& stored = session.questions.back();
  return make_bundle(session, stored);
}

SessionEngine::Next SessionEngineImpl::submit_result_adaptive(const std::string& session_id,
                                                              SessionData& session,
                                                              const ResultReport& report) {
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

  const bool session_complete =
      session.adaptive_target_questions != 0 &&
      session.result_log.size() >= session.adaptive_target_questions;

  if (session_complete) {
    if (!session.summary_ready) {
      session.summary_cache =
          build_summary(session_id, session.spec.drill_kind, session.result_log);
      session.summary_ready = true;
    }
    response = session.summary_cache;
    submit_cache.is_summary = true;
    submit_cache.summary = session.summary_cache;
  } else {
    auto bundle = make_bundle(session, state);
    response = bundle;
    submit_cache.is_summary = false;
    submit_cache.question = bundle;
  }

  session.submit_cache[report.question_id] = submit_cache;
  return response;
}

} // namespace ear
