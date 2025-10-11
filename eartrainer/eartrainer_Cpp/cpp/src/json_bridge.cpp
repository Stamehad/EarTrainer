#include "json_bridge.hpp"

#include <stdexcept>

namespace ear::bridge {
namespace {

// Convert the legacy PromptPlan (sequential notes with dur_ms) into a
// "midi-clip" JSON object ready for direct playback.
nlohmann::json prompt_plan_to_json_impl(const PromptPlan& plan) {
  const int tempo = plan.tempo_bpm.has_value() ? plan.tempo_bpm.value() : 90;
  const int ppq = 480;
  const double ticks_per_ms = (tempo * ppq) / 60000.0; // ticks = dur_ms * ticks_per_ms

  nlohmann::json clip = nlohmann::json::object();
  clip["format"] = "midi-clip/v1";
  clip["ppq"] = ppq;
  clip["tempo_bpm"] = tempo;

  nlohmann::json track = nlohmann::json::object();
  track["name"] = "prompt";
  track["channel"] = 0;
  track["program"] = 0; // GM Acoustic Grand
  nlohmann::json events = nlohmann::json::array();

  int t = 0; // timeline in ticks
  std::vector<int> held;

  if (plan.modality == "midi_block") {
    // Simultaneous note_on at t=0; individual offs based on dur_ms
    for (const auto& n : plan.notes) {
      int dur_ticks = static_cast<int>(std::lround(n.dur_ms * ticks_per_ms));
      if (n.pitch < 0 || dur_ticks <= 0) {
        continue;
      }
      int vel = n.vel.has_value() ? n.vel.value() : 90;
      nlohmann::json on = nlohmann::json::object();
      on["t"] = 0;
      on["type"] = "note_on";
      on["note"] = n.pitch;
      on["vel"] = std::max(0, std::min(127, vel));
      events.push_back(on);

      nlohmann::json off = nlohmann::json::object();
      off["t"] = std::max(1, dur_ticks);
      off["type"] = "note_off";
      off["note"] = n.pitch;
      events.push_back(off);
    }
    t = 0;
    // length ticks = max off time
    int max_off = 0;
    for (std::size_t i = 0; i < events.size(); ++i) {
      const auto& ev = events[i];
      if (ev["type"].get<std::string>() == "note_off") {
        max_off = std::max(max_off, ev["t"].get<int>());
      }
    }
    t = max_off;
  } else {
    for (const auto& n : plan.notes) {
      int dur_ticks = static_cast<int>(std::lround(n.dur_ms * ticks_per_ms));
      if (n.pitch < 0 || dur_ticks <= 0) {
        t += std::max(0, dur_ticks);
        continue;
      }
      int vel = n.vel.has_value() ? n.vel.value() : 90;
      nlohmann::json on = nlohmann::json::object();
      on["t"] = t;
      on["type"] = "note_on";
      on["note"] = n.pitch;
      on["vel"] = std::max(0, std::min(127, vel));
      events.push_back(on);

      bool tie_forward = n.tie.has_value() && n.tie.value();
      if (!tie_forward) {
        nlohmann::json off = nlohmann::json::object();
        off["t"] = t + std::max(1, dur_ticks);
        off["type"] = "note_off";
        off["note"] = n.pitch;
        events.push_back(off);
      } else {
        held.push_back(n.pitch);
      }
      t += std::max(1, dur_ticks);
    }
    // Ensure all held notes are released at the end
    for (int pitch : held) {
      nlohmann::json off = nlohmann::json::object();
      off["t"] = t;
      off["type"] = "note_off";
      off["note"] = pitch;
      events.push_back(off);
    }
  }

  track["events"] = events;
  clip["length_ticks"] = t;
  nlohmann::json tracks = nlohmann::json::array();
  tracks.push_back(track);
  clip["tracks"] = tracks;

  nlohmann::json json_plan = nlohmann::json::object();
  json_plan["modality"] = "midi-clip";
  json_plan["midi_clip"] = clip;
  return json_plan;
}

PromptPlan prompt_from_json_impl(const nlohmann::json& json_plan) {
  // Legacy loader retained only for round-trips if needed. Not used by Python UI.
  PromptPlan plan;
  if (json_plan.contains("modality")) {
    plan.modality = json_plan["modality"].get<std::string>();
  } else {
    plan.modality = "midi-clip";
  }
  // We no longer reconstruct sequential notes from midi-clip here.
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
  if (!spec.track_levels.empty()) {
    nlohmann::json track_levels = nlohmann::json::array();
    for (int level : spec.track_levels) track_levels.push_back(level);
    json_spec["track_levels"] = track_levels;
  }
  json_spec["seed"] = static_cast<std::int64_t>(spec.seed);
  json_spec["adaptive"] = spec.adaptive;
  nlohmann::json track_levels = nlohmann::json::array();
  for (int level : spec.track_levels) {
    track_levels.push_back(level);
  }
  json_spec["track_levels"] = track_levels;
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
  if (json_spec.contains("track_levels")) {
    const auto& levels = json_spec["track_levels"];
    if (levels.is_array()) {
      spec.track_levels.clear();
      for (const auto& entry : levels.get_array()) {
        if (entry.is_number_integer()) {
          spec.track_levels.push_back(entry.get<int>());
        }
      }
    }
  }
  spec.seed = static_cast<std::uint64_t>(json_spec["seed"].get<long long>());
  if (json_spec.contains("adaptive")) {
    spec.adaptive = json_spec["adaptive"].get<bool>();
  }
  if (json_spec.contains("track_levels")) {
    const auto& levels = json_spec["track_levels"];
    if (levels.is_array()) {
      spec.track_levels.clear();
      for (const auto& entry : levels.get_array()) {
        if (entry.is_number_integer()) {
          spec.track_levels.push_back(entry.get<int>());
        }
      }
    }
  }
  return spec;
}

nlohmann::json to_json(const QuestionBundle& bundle) {
  nlohmann::json json_bundle = nlohmann::json::object();
  json_bundle["question_id"] = bundle.question_id;
  json_bundle["question"] = typed_to_json(bundle.question);
  json_bundle["correct_answer"] = typed_to_json(bundle.correct_answer);
  if (bundle.prompt.has_value()) {
    json_bundle["prompt"] = prompt_plan_to_json_impl(bundle.prompt.value());
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
    bundle.prompt = prompt_from_json_impl(json_bundle["prompt"]);
  }
  bundle.ui_hints = json_bundle["ui_hints"];
  return bundle;
}

nlohmann::json to_json(const AssistBundle& bundle) {
  nlohmann::json json_bundle = nlohmann::json::object();
  json_bundle["question_id"] = bundle.question_id;
  json_bundle["kind"] = bundle.kind;
  if (bundle.prompt.has_value()) {
    json_bundle["prompt"] = prompt_plan_to_json_impl(bundle.prompt.value());
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
    bundle.prompt = prompt_from_json_impl(json_bundle["prompt"]);
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
  metrics["question_count"] = report.metrics.question_count;
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
  if (metrics.contains("question_count")) {
    report.metrics.question_count = metrics["question_count"].get<int>();
  } else {
    report.metrics.question_count = 1;
  }
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

nlohmann::json to_json(const MemoryPackage& package) {
  nlohmann::json json_package = nlohmann::json::object();
  json_package["summary"] = to_json(package.summary);
  if (package.adaptive.has_value()) {
    const auto& adaptive = package.adaptive.value();
    nlohmann::json json_adaptive = nlohmann::json::object();
    json_adaptive["has_score"] = adaptive.has_score;
    json_adaptive["bout_average"] = adaptive.bout_average;
    json_adaptive["graduate_threshold"] = adaptive.graduate_threshold;
    json_adaptive["level_up"] = adaptive.level_up;
    nlohmann::json drills = nlohmann::json::object();
    for (const auto& entry : adaptive.drills) {
      nlohmann::json drill = nlohmann::json::object();
      drill["family"] = entry.second.family;
      if (entry.second.ema_score.has_value()) {
        drill["ema_score"] = entry.second.ema_score.value();
      } else {
        drill["ema_score"] = nullptr;
      }
      drills[entry.first] = std::move(drill);
    }
    json_adaptive["drills"] = std::move(drills);
    if (adaptive.level.has_value()) {
      const auto& level = adaptive.level.value();
      nlohmann::json json_level = nlohmann::json::object();
      json_level["track_index"] = level.track_index;
      json_level["track_name"] = level.track_name;
      json_level["current_level"] = level.current_level;
      if (level.suggested_level.has_value()) {
        json_level["suggested_level"] = level.suggested_level.value();
      } else {
        json_level["suggested_level"] = nullptr;
      }
      json_adaptive["level"] = std::move(json_level);
    } else {
      json_adaptive["level"] = nullptr;
    }
    json_package["adaptive"] = std::move(json_adaptive);
  } else {
    json_package["adaptive"] = nullptr;
  }
  return json_package;
}

nlohmann::json to_json(const PromptPlan& plan) {
  return prompt_plan_to_json_impl(plan);
}

PromptPlan prompt_plan_from_json(const nlohmann::json& json_plan) {
  return prompt_from_json_impl(json_plan);
}

} // namespace ear::bridge
