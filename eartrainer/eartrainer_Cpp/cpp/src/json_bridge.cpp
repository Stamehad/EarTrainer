#include "json_bridge.hpp"

#include <stdexcept>

namespace ear::bridge {
namespace {

nlohmann::json to_json(const PromptPlan& plan) {
  nlohmann::json json_plan = nlohmann::json::object();
  json_plan["modality"] = plan.modality;
  nlohmann::json notes = nlohmann::json::array();
  for (const auto& note : plan.notes) {
    nlohmann::json note_json = nlohmann::json::object();
    note_json["pitch"] = note.pitch;
    note_json["dur_ms"] = note.dur_ms;
    if (note.vel.has_value()) {
      note_json["vel"] = note.vel.value();
    } else {
      note_json["vel"] = nullptr;
    }
    if (note.tie.has_value()) {
      note_json["tie"] = note.tie.value();
    } else {
      note_json["tie"] = nullptr;
    }
    notes.push_back(note_json);
  }
  json_plan["notes"] = notes;
  if (plan.tempo_bpm.has_value()) {
    json_plan["tempo_bpm"] = plan.tempo_bpm.value();
  } else {
    json_plan["tempo_bpm"] = nullptr;
  }
  if (plan.count_in.has_value()) {
    json_plan["count_in"] = plan.count_in.value();
  } else {
    json_plan["count_in"] = nullptr;
  }
  return json_plan;
}

PromptPlan prompt_from_json(const nlohmann::json& json_plan) {
  PromptPlan plan;
  plan.modality = json_plan["modality"].get<std::string>();
  const auto& notes = json_plan["notes"].get_array();
  for (const auto& note_json : notes) {
    Note note;
    note.pitch = note_json["pitch"].get<int>();
    note.dur_ms = note_json["dur_ms"].get<int>();
    if (!note_json["vel"].is_null()) {
      note.vel = note_json["vel"].get<int>();
    }
    if (!note_json["tie"].is_null()) {
      note.tie = note_json["tie"].get<bool>();
    }
    plan.notes.push_back(note);
  }
  if (!json_plan["tempo_bpm"].is_null()) {
    plan.tempo_bpm = json_plan["tempo_bpm"].get<int>();
  }
  if (!json_plan["count_in"].is_null()) {
    plan.count_in = json_plan["count_in"].get<bool>();
  }
  return plan;
}

nlohmann::json typed_to_json(const TypedPayload& payload) {
  nlohmann::json json_payload = nlohmann::json::object();
  json_payload["type"] = payload.type;
  json_payload["payload"] = payload.payload;
  return json_payload;
}

TypedPayload typed_from_json(const nlohmann::json& json_payload) {
  TypedPayload payload;
  payload.type = json_payload["type"].get<std::string>();
  payload.payload = json_payload["payload"];
  return payload;
}

} // namespace

nlohmann::json to_json(const SessionSpec& spec) {
  nlohmann::json json_spec = nlohmann::json::object();
  json_spec["version"] = spec.version;
  json_spec["drill_kind"] = spec.drill_kind;
  json_spec["key"] = spec.key;
  nlohmann::json range = nlohmann::json::array();
  range.push_back(spec.range_min);
  range.push_back(spec.range_max);
  json_spec["range"] = range;
  if (spec.tempo_bpm.has_value()) {
    json_spec["tempo_bpm"] = spec.tempo_bpm.value();
  } else {
    json_spec["tempo_bpm"] = nullptr;
  }
  json_spec["n_questions"] = spec.n_questions;
  json_spec["generation"] = spec.generation;

  nlohmann::json assistance = nlohmann::json::object();
  for (const auto& entry : spec.assistance_policy) {
    assistance[entry.first] = entry.second;
  }
  json_spec["assistance_policy"] = assistance;
  json_spec["sampler_params"] = spec.sampler_params;
  json_spec["seed"] = static_cast<std::int64_t>(spec.seed);
  return json_spec;
}

SessionSpec session_spec_from_json(const nlohmann::json& json_spec) {
  SessionSpec spec;
  spec.version = json_spec["version"].get<std::string>();
  spec.drill_kind = json_spec["drill_kind"].get<std::string>();
  spec.key = json_spec["key"].get<std::string>();
  const auto& range = json_spec["range"].get_array();
  if (range.size() != 2) {
    throw std::runtime_error("range must have two elements");
  }
  spec.range_min = range[0].get<int>();
  spec.range_max = range[1].get<int>();
  if (!json_spec["tempo_bpm"].is_null()) {
    spec.tempo_bpm = json_spec["tempo_bpm"].get<int>();
  }
  spec.n_questions = json_spec["n_questions"].get<int>();
  spec.generation = json_spec["generation"].get<std::string>();
  spec.assistance_policy.clear();
  const auto& assistance = json_spec["assistance_policy"].get_object();
  for (const auto& entry : assistance) {
    spec.assistance_policy[entry.first] = entry.second.get<int>();
  }
  spec.sampler_params = json_spec.value("sampler_params", nlohmann::json::object());
  spec.seed = static_cast<std::uint64_t>(json_spec["seed"].get<long long>());
  return spec;
}

nlohmann::json to_json(const QuestionBundle& bundle) {
  nlohmann::json json_bundle = nlohmann::json::object();
  json_bundle["question_id"] = bundle.question_id;
  json_bundle["question"] = typed_to_json(bundle.question);
  json_bundle["correct_answer"] = typed_to_json(bundle.correct_answer);
  if (bundle.prompt.has_value()) {
    json_bundle["prompt"] = to_json(bundle.prompt.value());
  } else {
    json_bundle["prompt"] = nullptr;
  }
  json_bundle["ui_hints"] = bundle.ui_hints;
  return json_bundle;
}

QuestionBundle question_bundle_from_json(const nlohmann::json& json_bundle) {
  QuestionBundle bundle;
  bundle.question_id = json_bundle["question_id"].get<std::string>();
  bundle.question = typed_from_json(json_bundle["question"]);
  bundle.correct_answer = typed_from_json(json_bundle["correct_answer"]);
  if (!json_bundle["prompt"].is_null()) {
    bundle.prompt = prompt_from_json(json_bundle["prompt"]);
  }
  bundle.ui_hints = json_bundle["ui_hints"];
  return bundle;
}

nlohmann::json to_json(const AssistBundle& bundle) {
  nlohmann::json json_bundle = nlohmann::json::object();
  json_bundle["question_id"] = bundle.question_id;
  json_bundle["kind"] = bundle.kind;
  if (bundle.prompt.has_value()) {
    json_bundle["prompt"] = to_json(bundle.prompt.value());
  } else {
    json_bundle["prompt"] = nullptr;
  }
  json_bundle["ui_delta"] = bundle.ui_delta;
  return json_bundle;
}

AssistBundle assist_bundle_from_json(const nlohmann::json& json_bundle) {
  AssistBundle bundle;
  bundle.question_id = json_bundle["question_id"].get<std::string>();
  bundle.kind = json_bundle["kind"].get<std::string>();
  if (!json_bundle["prompt"].is_null()) {
    bundle.prompt = prompt_from_json(json_bundle["prompt"]);
  }
  bundle.ui_delta = json_bundle["ui_delta"];
  return bundle;
}

nlohmann::json to_json(const ResultReport& report) {
  nlohmann::json json_report = nlohmann::json::object();
  json_report["question_id"] = report.question_id;
  json_report["final_answer"] = typed_to_json(report.final_answer);
  json_report["correct"] = report.correct;
  nlohmann::json metrics = nlohmann::json::object();
  metrics["rt_ms"] = report.metrics.rt_ms;
  metrics["attempts"] = report.metrics.attempts;
  nlohmann::json assists = nlohmann::json::object();
  for (const auto& entry : report.metrics.assists_used) {
    assists[entry.first] = entry.second;
  }
  metrics["assists_used"] = assists;
  if (report.metrics.first_input_rt_ms.has_value()) {
    metrics["first_input_rt_ms"] = report.metrics.first_input_rt_ms.value();
  } else {
    metrics["first_input_rt_ms"] = nullptr;
  }
  json_report["metrics"] = metrics;
  return json_report;
}

ResultReport result_report_from_json(const nlohmann::json& json_report) {
  ResultReport report;
  report.question_id = json_report["question_id"].get<std::string>();
  report.final_answer = typed_from_json(json_report["final_answer"]);
  report.correct = json_report["correct"].get<bool>();
  const auto& metrics = json_report["metrics"];
  report.metrics.rt_ms = metrics["rt_ms"].get<int>();
  report.metrics.attempts = metrics["attempts"].get<int>();
  report.metrics.assists_used.clear();
  const auto& assists = metrics["assists_used"].get_object();
  for (const auto& entry : assists) {
    report.metrics.assists_used[entry.first] = entry.second.get<int>();
  }
  if (!metrics["first_input_rt_ms"].is_null()) {
    report.metrics.first_input_rt_ms = metrics["first_input_rt_ms"].get<int>();
  }
  return report;
}

nlohmann::json to_json(const SessionSummary& summary) {
  nlohmann::json json_summary = nlohmann::json::object();
  json_summary["session_id"] = summary.session_id;
  json_summary["totals"] = summary.totals;
  json_summary["by_category"] = summary.by_category;
  json_summary["results"] = summary.results;
  return json_summary;
}

SessionSummary session_summary_from_json(const nlohmann::json& json_summary) {
  SessionSummary summary;
  summary.session_id = json_summary["session_id"].get<std::string>();
  summary.totals = json_summary["totals"];
  summary.by_category = json_summary["by_category"];
  summary.results = json_summary["results"];
  return summary;
}

} // namespace ear::bridge

