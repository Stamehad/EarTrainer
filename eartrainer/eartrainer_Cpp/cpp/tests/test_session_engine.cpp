#include "../include/ear/session_engine.hpp"
#include "../include/ear/track_selector.hpp"
#include "../include/ear/types.hpp"
#include "../include/ear/question_bundle_v2.hpp"
#include "scoring/scoring.hpp"

#include "../src/json_bridge.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <type_traits>
#include <variant>
#include <vector>

namespace {

struct TestSuite {
  bool ok = true;
  void require(bool condition, const std::string& message) {
    if (!condition) {
      std::cerr << "[FAIL] " << message << std::endl;
      ok = false;
    }
  }
};

std::string key_quality_to_string(ear::KeyQuality quality) {
  switch (quality) {
    case ear::KeyQuality::Major:
      return "major";
    case ear::KeyQuality::Minor:
      return "minor";
  }
  return "major";
}

std::string triad_quality_to_string(ear::TriadQuality quality) {
  switch (quality) {
    case ear::TriadQuality::Major:
      return "major";
    case ear::TriadQuality::Minor:
      return "minor";
    case ear::TriadQuality::Diminished:
      return "diminished";
  }
  return "major";
}

std::string answer_kind_to_string(ear::AnswerKind kind) {
  switch (kind) {
    case ear::AnswerKind::ChordDegree:
      return "ChordDegree";
  }
  return "Unknown";
}

ear::KeyQuality key_quality_from_string(const std::string& key) {
  return key == "minor" ? ear::KeyQuality::Minor : ear::KeyQuality::Major;
}

ear::TriadQuality triad_quality_from_string(const std::string& quality) {
  if (quality == "minor") {
    return ear::TriadQuality::Minor;
  }
  if (quality == "diminished") {
    return ear::TriadQuality::Diminished;
  }
  return ear::TriadQuality::Major;
}

ear::AnswerKind answer_kind_from_string(const std::string& kind) {
  if (kind == "ChordDegree") {
    return ear::AnswerKind::ChordDegree;
  }
  return ear::AnswerKind::ChordDegree;
}

nlohmann::json question_payload_json(const ear::QuestionPayloadV2& payload) {
  return std::visit(
      [](const auto& q) -> nlohmann::json {
        using T = std::decay_t<decltype(q)>;
        nlohmann::json json = nlohmann::json::object();
        if constexpr (std::is_same_v<T, ear::ChordQuestionV2>) {
          json["type"] = "chord";
          json["tonic_midi"] = q.tonic_midi;
          json["tonic"] = q.tonic;
          json["key"] = key_quality_to_string(q.key);
          json["root_degree"] = q.root_degree;
          json["quality"] = triad_quality_to_string(q.quality);
          if (q.rh_degrees.has_value()) {
            nlohmann::json arr = nlohmann::json::array();
            for (int d : q.rh_degrees.value()) arr.push_back(d);
            json["rh_degrees"] = std::move(arr);
          } else {
            json["rh_degrees"] = nullptr;
          }
          if (q.bass_degrees.has_value()) {
            json["bass_degrees"] = q.bass_degrees.value();
          } else {
            json["bass_degrees"] = nullptr;
          }
          if (q.right_voicing_id.has_value()) {
            json["right_voicing_id"] = q.right_voicing_id.value();
          } else {
            json["right_voicing_id"] = nullptr;
          }
          if (q.bass_voicing_id.has_value()) {
            json["bass_voicing_id"] = q.bass_voicing_id.value();
          } else {
            json["bass_voicing_id"] = nullptr;
          }
        } else if constexpr (std::is_same_v<T, ear::MelodyQuestionV2>) {
          json["type"] = "melody";
          json["tonic_midi"] = q.tonic_midi;
          json["tonic"] = q.tonic;
          json["key"] = key_quality_to_string(q.key);
          {
            nlohmann::json arr = nlohmann::json::array();
            for (int d : q.melody) arr.push_back(d);
            json["melody"] = std::move(arr);
          }
          if (q.octave.has_value()) {
            nlohmann::json arr = nlohmann::json::array();
            for (int d : q.octave.value()) arr.push_back(d);
            json["octave"] = std::move(arr);
          } else {
            json["octave"] = nullptr;
          }
          if (q.helper.has_value()) {
            json["helper"] = q.helper.value();
          } else {
            json["helper"] = nullptr;
          }
        } else if constexpr (std::is_same_v<T, ear::HarmonyQuestionV2>) {
          json["type"] = "harmony";
          json["tonic_midi"] = q.tonic_midi;
          json["tonic"] = q.tonic;
          json["key"] = key_quality_to_string(q.key);
          json["note_num"] = q.note_num;
          {
            nlohmann::json arr = nlohmann::json::array();
            for (int n : q.notes) arr.push_back(n);
            json["notes"] = std::move(arr);
          }
          if (q.interval.has_value()) {
            json["interval"] = q.interval.value();
          } else {
            json["interval"] = nullptr;
          }
        }
        return json;
      },
      payload);
}

nlohmann::json answer_payload_json(const ear::AnswerPayloadV2& payload);

ear::QuestionPayloadV2 question_payload_from_json(const nlohmann::json& json) {
  const std::string type = json["type"].get<std::string>();
  if (type == "chord") {
    ear::ChordQuestionV2 q;
    q.tonic_midi = json["tonic_midi"].get<int>();
    q.tonic = json["tonic"].get<std::string>();
    q.key = key_quality_from_string(json["key"].get<std::string>());
    q.root_degree = json["root_degree"].get<int>();
    q.quality = triad_quality_from_string(json["quality"].get<std::string>());
    if (!json["rh_degrees"].is_null()) {
      std::vector<int> vals;
      if (json["rh_degrees"].is_array()) {
        for (const auto& el : json["rh_degrees"].get_array()) {
          vals.push_back(el.get<int>());
        }
      }
      q.rh_degrees = std::move(vals);
    }
    if (!json["bass_degrees"].is_null()) {
      q.bass_degrees = json["bass_degrees"].get<int>();
    }
    if (!json["right_voicing_id"].is_null()) {
      q.right_voicing_id = json["right_voicing_id"].get<std::string>();
    }
    if (!json["bass_voicing_id"].is_null()) {
      q.bass_voicing_id = json["bass_voicing_id"].get<std::string>();
    }
    return q;
  }
  if (type == "melody") {
    ear::MelodyQuestionV2 q;
    q.tonic_midi = json["tonic_midi"].get<int>();
    q.tonic = json["tonic"].get<std::string>();
    q.key = key_quality_from_string(json["key"].get<std::string>());
    q.melody.clear();
    if (json["melody"].is_array()) {
      for (const auto& el : json["melody"].get_array()) q.melody.push_back(el.get<int>());
    }
    if (!json["octave"].is_null()) {
      std::vector<int> vals;
      if (json["octave"].is_array()) {
        for (const auto& el : json["octave"].get_array()) vals.push_back(el.get<int>());
      }
      q.octave = std::move(vals);
    }
    if (!json["helper"].is_null()) {
      q.helper = json["helper"].get<std::string>();
    }
    return q;
  }
  if (type == "harmony") {
    ear::HarmonyQuestionV2 q;
    q.tonic_midi = json["tonic_midi"].get<int>();
    q.tonic = json["tonic"].get<std::string>();
    q.key = key_quality_from_string(json["key"].get<std::string>());
    q.note_num = json["note_num"].get<int>();
    q.notes.clear();
    if (json["notes"].is_array()) {
      for (const auto& el : json["notes"].get_array()) q.notes.push_back(el.get<int>());
    }
    if (!json["interval"].is_null()) {
      q.interval = json["interval"].get<std::string>();
    }
    return q;
  }
  throw std::runtime_error("Unknown question payload type: " + type);
}

nlohmann::json answer_payload_json(const ear::AnswerPayloadV2& payload) {
  return std::visit(
      [](const auto& a) -> nlohmann::json {
        using T = std::decay_t<decltype(a)>;
        nlohmann::json json = nlohmann::json::object();
        if constexpr (std::is_same_v<T, ear::ChordAnswerV2>) {
          json["type"] = "chord";
          json["root_degree"] = a.root_degree;
          if (a.bass_deg.has_value()) {
            json["bass_deg"] = a.bass_deg.value();
          } else {
            json["bass_deg"] = nullptr;
          }
          if (a.top_deg.has_value()) {
            json["top_deg"] = a.top_deg.value();
          } else {
            json["top_deg"] = nullptr;
          }
        } else if constexpr (std::is_same_v<T, ear::MelodyAnswerV2>) {
          json["type"] = "melody";
          {
            nlohmann::json arr = nlohmann::json::array();
            for (int d : a.melody) arr.push_back(d);
            json["melody"] = std::move(arr);
          }
        } else if constexpr (std::is_same_v<T, ear::HarmonyAnswerV2>) {
          json["type"] = "harmony";
          {
            nlohmann::json arr = nlohmann::json::array();
            for (int n : a.notes) arr.push_back(n);
            json["notes"] = std::move(arr);
          }
        }
        return json;
      },
      payload);
}

ear::AnswerPayloadV2 answer_payload_from_json(const nlohmann::json& json) {
  const std::string type = json["type"].get<std::string>();
  if (type == "chord") {
    ear::ChordAnswerV2 answer;
    answer.root_degree = json["root_degree"].get<int>();
    if (!json["bass_deg"].is_null()) {
      answer.bass_deg = json["bass_deg"].get<int>();
    }
    if (!json["top_deg"].is_null()) {
      answer.top_deg = json["top_deg"].get<int>();
    }
    return answer;
  }
  if (type == "melody") {
    ear::MelodyAnswerV2 answer;
    answer.melody.clear();
    if (json["melody"].is_array()) {
      for (const auto& el : json["melody"].get_array()) {
        answer.melody.push_back(el.get<int>());
      }
    }
    return answer;
  }
  if (type == "harmony") {
    ear::HarmonyAnswerV2 answer;
    answer.notes.clear();
    if (json["notes"].is_array()) {
      for (const auto& el : json["notes"].get_array()) {
        answer.notes.push_back(el.get<int>());
      }
    }
    return answer;
  }
  throw std::runtime_error("Unknown answer payload type: " + type);
}

nlohmann::json ui_hints_json(const std::optional<ear::UiHintsV2>& hints) {
  if (!hints.has_value()) {
    return nullptr;
  }
  nlohmann::json json = nlohmann::json::object();
  json["answer_kind"] = answer_kind_to_string(hints->answer_kind);
  {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& s : hints->allowed_assists) arr.push_back(s);
    json["allowed_assists"] = std::move(arr);
  }
  return json;
}

std::optional<ear::UiHintsV2> ui_hints_from_json(const nlohmann::json& json) {
  if (json.is_null()) {
    return std::nullopt;
  }
  ear::UiHintsV2 hints;
  hints.answer_kind = answer_kind_from_string(json["answer_kind"].get<std::string>());
  hints.allowed_assists.clear();
  if (json["allowed_assists"].is_array()) {
    for (const auto& el : json["allowed_assists"].get_array()) {
      hints.allowed_assists.push_back(el.get<std::string>());
    }
  }
  return hints;
}

ear::MidiClip midi_clip_from_json(const nlohmann::json& json) {
  ear::MidiClip clip;
  clip.format = json.contains("format") ? json["format"].get<std::string>() : "midi-clip/v1";
  clip.ppq = json["ppq"].get<int>();
  clip.tempo_bpm = json["tempo_bpm"].get<int>();
  clip.length_ticks = json["length_ticks"].get<int>();
  clip.tracks.clear();
  if (json.contains("tracks") && json["tracks"].is_array()) {
    for (const auto& track_json : json["tracks"].get_array()) {
      ear::MidiTrack track;
      track.name = track_json["name"].get<std::string>();
      track.channel = track_json["channel"].get<int>();
      track.program = track_json["program"].get<int>();
      if (track_json.contains("events") && track_json["events"].is_array()) {
        for (const auto& event_json : track_json["events"].get_array()) {
          ear::MidiEvent event;
          event.t = event_json["t"].get<int>();
          event.type = event_json["type"].get<std::string>();
          if (event_json.contains("note")) {
            event.note = event_json["note"].get<int>();
          }
          if (event_json.contains("vel")) {
            event.vel = event_json["vel"].get<int>();
          }
          if (event_json.contains("control")) {
            event.control = event_json["control"].get<int>();
          }
          if (event_json.contains("value")) {
            event.value = event_json["value"].get<int>();
          }
          track.events.push_back(event);
        }
      }
      clip.tracks.push_back(std::move(track));
    }
  }
  return clip;
}

ear::SessionSpec make_spec(const std::string& drill_kind, const std::string& generation,
                           std::uint64_t seed, int n_questions = 3) {
  ear::SessionSpec spec;
  spec.version = "v1";
  spec.drill_kind = drill_kind;
  spec.key = "C major";
  spec.tempo_bpm = 90;
  spec.n_questions = n_questions;
  spec.generation = generation;
  spec.assistance_policy = {{"GuideTone", 2}, {"Replay", 99}};
  spec.sampler_params = nlohmann::json::object();
  spec.seed = seed;
  return spec;
}

  ear::ResultReport make_report(const ear::QuestionBundle& bundle, bool correct = true) {
  ear::ResultReport report;
  report.question_id = bundle.question_id;
  report.final_answer = bundle.correct_answer;
  report.correct = correct;
  report.metrics.rt_ms = 900;
  report.metrics.attempts = 1;
  report.metrics.assists_used = {};
  report.metrics.first_input_rt_ms = 600;
  return report;
}

  nlohmann::json question_bundle_json(const ear::QuestionBundle& bundle) {
  nlohmann::json json_bundle = nlohmann::json::object();
  json_bundle["question_id"] = bundle.question_id;
  json_bundle["question"] = question_payload_json(bundle.question);
  json_bundle["correct_answer"] = answer_payload_json(bundle.correct_answer);
  if (bundle.prompt_clip.has_value()) {
    json_bundle["prompt_clip"] = ear::to_json(bundle.prompt_clip.value());
  } else {
    json_bundle["prompt_clip"] = nullptr;
  }
  json_bundle["ui_hints"] = ui_hints_json(bundle.ui_hints);
  return json_bundle;
}

  ear::QuestionBundle question_bundle_from_json(const nlohmann::json& json_bundle) {
  ear::QuestionBundle bundle;
  bundle.question_id = json_bundle["question_id"].get<std::string>();
  bundle.question = question_payload_from_json(json_bundle["question"]);
  bundle.correct_answer = answer_payload_from_json(json_bundle["correct_answer"]);
  if (json_bundle.contains("prompt_clip") && !json_bundle["prompt_clip"].is_null()) {
    bundle.prompt_clip = midi_clip_from_json(json_bundle["prompt_clip"]);
  }
  bundle.ui_hints = ui_hints_from_json(json_bundle.value("ui_hints", nlohmann::json(nullptr)));
  return bundle;
}

  std::string digest(const ear::QuestionBundle& bundle) {
  return question_bundle_json(bundle).dump();
}

void test_track_selector(TestSuite& suite) {
  using ear::adaptive::TrackCatalogDescriptor;
  using ear::adaptive::compute_track_phase_weights;

  const auto base_dir = std::filesystem::path(__FILE__).parent_path() / "catalogs";
  std::vector<TrackCatalogDescriptor> catalog_descriptors = {
      {"degree", base_dir / "degree_levels_test.json"},
      {"melody", base_dir / "melody_levels_test.json"},
      {"chord",  base_dir / "chord_levels_test.json"},
  };
  auto catalogs = ear::adaptive::load_track_phase_catalogs(catalog_descriptors);

  {
    auto result = compute_track_phase_weights(std::vector<int>{11, 111, 211}, catalogs);
    suite.require(result.phase_digit == 1, "expected phase digit 1 for initial phase");
    suite.require(result.weights.size() == catalogs.size(), "weight count should match tracks");
    suite.require(result.weights[0] == 3, "degree track should have 3 pending levels");
    suite.require(result.weights[1] == 2, "melody track should have 2 pending levels");
    suite.require(result.weights[2] == 3, "chord track should have 3 pending levels");
  }

  {
    auto result = compute_track_phase_weights(std::vector<int>{19, 118, 211}, catalogs);
    suite.require(result.weights[0] == 1, "degree track should have 1 pending level after progress");
    suite.require(result.weights[1] == 1, "melody track should have 1 pending level after progress");
    suite.require(result.weights[2] == 3, "chord track remains at 3 pending levels");
  }

  {
    auto result = compute_track_phase_weights(std::vector<int>{21, 118, 217}, catalogs);
    suite.require(result.phase_digit == 1, "phase should remain 1 until all phase-1 levels complete");
    suite.require(result.weights[0] == 0, "track ahead of phase should contribute zero weight");
    suite.require(result.weights[1] == 1, "melody track should have 1 pending level");
    suite.require(result.weights[2] == 1, "chord track should have 1 pending level");
  }

  {
    auto result = compute_track_phase_weights(std::vector<int>{0, 0, 0}, catalogs);
    suite.require(result.phase_digit == -1, "no active phase when all tracks finished");
    suite.require(std::all_of(result.weights.begin(), result.weights.end(),
                              [](int weight) { return weight == 0; }),
                  "weights should be zero when no track is active");
  }

  bool mismatched_sizes_threw = false;
  try {
    (void)compute_track_phase_weights(std::vector<int>{11, 111}, catalogs);
  } catch (const std::invalid_argument&) {
    mismatched_sizes_threw = true;
  }
  suite.require(mismatched_sizes_threw, "size mismatch should raise invalid_argument");

  bool missing_level_threw = false;
  try {
    (void)compute_track_phase_weights(std::vector<int>{12, 111, 211}, catalogs);
  } catch (const std::runtime_error&) {
    missing_level_threw = true;
  }
  suite.require(missing_level_threw, "unknown current level should raise runtime_error");
}

} // namespace

int main() {
  TestSuite suite;

  test_track_selector(suite);

  {
    auto engine1 = ear::make_engine();
    auto spec = make_spec("note", "eager", 123, 3);
    auto session1 = engine1->create_session(spec);
    std::vector<std::string> seq1;
    for (int i = 0; i < spec.n_questions; ++i) {
      auto next = engine1->next_question(session1);
      const auto* bundle = std::get_if<ear::QuestionBundle>(&next);
      suite.require(bundle != nullptr, "next_question should return QuestionsBundle");
      seq1.push_back(digest(*bundle));
      engine1->submit_result(session1, make_report(*bundle));
    }

    auto engine2 = ear::make_engine();
    auto session2 = engine2->create_session(spec);
    std::vector<std::string> seq2;
    for (int i = 0; i < spec.n_questions; ++i) {
      auto next = engine2->next_question(session2);
      const auto* bundle = std::get_if<ear::QuestionBundle>(&next);
      suite.require(bundle != nullptr, "next_question should return QuestionsBundle (run 2)");
      seq2.push_back(digest(*bundle));
      engine2->submit_result(session2, make_report(*bundle));
    }

    suite.require(seq1 == seq2, "Determinism failure: eager mode sequences differ");
  }

  {
    auto engine1 = ear::make_engine();
    auto spec = make_spec("interval", "eager", 432, 3);
    auto session1 = engine1->create_session(spec);
    std::vector<std::string> seq1;
    for (int i = 0; i < spec.n_questions; ++i) {
      auto next = engine1->next_question(session1);
      const auto* bundle = std::get_if<ear::QuestionBundle>(&next);
      suite.require(bundle != nullptr, "interval next_question should return QuestionsBundle");
      seq1.push_back(digest(*bundle));
      engine1->submit_result(session1, make_report(*bundle));
    }

    auto engine2 = ear::make_engine();
    auto session2 = engine2->create_session(spec);
    std::vector<std::string> seq2;
    for (int i = 0; i < spec.n_questions; ++i) {
      auto next = engine2->next_question(session2);
      const auto* bundle = std::get_if<ear::QuestionBundle>(&next);
      suite.require(bundle != nullptr,
                    "interval next_question should return QuestionsBundle (run 2)");
      seq2.push_back(digest(*bundle));
      engine2->submit_result(session2, make_report(*bundle));
    }

    suite.require(seq1 == seq2, "Determinism failure: interval eager mode sequences differ");
  }

  {
    auto engine1 = ear::make_engine();
    auto spec = make_spec("melody", "eager", 789, 3);
    auto session1 = engine1->create_session(spec);
    std::vector<std::string> seq1;
    for (int i = 0; i < spec.n_questions; ++i) {
      auto next = engine1->next_question(session1);
      const auto* bundle = std::get_if<ear::QuestionBundle>(&next);
      suite.require(bundle != nullptr, "melody next_question should return QuestionsBundle");
      seq1.push_back(digest(*bundle));
      engine1->submit_result(session1, make_report(*bundle));
    }

    auto engine2 = ear::make_engine();
    auto session2 = engine2->create_session(spec);
    std::vector<std::string> seq2;
    for (int i = 0; i < spec.n_questions; ++i) {
      auto next = engine2->next_question(session2);
      const auto* bundle = std::get_if<ear::QuestionBundle>(&next);
      suite.require(bundle != nullptr,
                    "melody next_question should return QuestionsBundle (run 2)");
      seq2.push_back(digest(*bundle));
      engine2->submit_result(session2, make_report(*bundle));
    }

    suite.require(seq1 == seq2, "Determinism failure: melody eager mode sequences differ");
  }

  {
    auto engine = ear::make_engine();
    auto spec = make_spec("note", "adaptive", 77, 2);
    auto session = engine->create_session(spec);

    auto first_next = engine->next_question(session);
    auto* bundle = std::get_if<ear::QuestionBundle>(&first_next);
    suite.require(bundle != nullptr, "Expected QuestionsBundle for melody test");
    auto original_digest = digest(*bundle);

    auto assist_bundle = engine->assist(session, bundle->question_id, "GuideTone");
    suite.require(assist_bundle.question_id == bundle->question_id,
                  "Assist bundle mismatch question id");

    auto repeat_next = engine->next_question(session);
    auto* repeat_bundle = std::get_if<ear::QuestionBundle>(&repeat_next);
    suite.require(repeat_bundle != nullptr, "Expected QuestionsBundle after assist");
    suite.require(digest(*repeat_bundle) == original_digest,
                  "Assist modified active question");

    auto first_submit = engine->submit_result(session, make_report(*repeat_bundle));
    auto* submit_bundle = std::get_if<ear::QuestionBundle>(&first_submit);
    suite.require(submit_bundle != nullptr, "Expected QuestionsBundle response on submit");

    auto second_submit = engine->submit_result(session, make_report(*repeat_bundle));
    auto* submit_bundle_repeat = std::get_if<ear::QuestionBundle>(&second_submit);
    suite.require(submit_bundle_repeat != nullptr,
                  "Expected idempotent submit to return QuestionsBundle");
    suite.require(digest(*submit_bundle) == digest(*submit_bundle_repeat),
                  "Idempotent submit changed response");
  }

  {
    auto spec = make_spec("note", "adaptive", 555, 2);
    auto engine_a = ear::make_engine();
    auto session_a = engine_a->create_session(spec);
    auto first_a = std::get<ear::QuestionBundle>(engine_a->next_question(session_a));
    engine_a->assist(session_a, first_a.question_id, "GuideTone");
    engine_a->submit_result(session_a, make_report(first_a));
    auto second_a = std::get<ear::QuestionBundle>(engine_a->next_question(session_a));

    auto engine_b = ear::make_engine();
    auto session_b = engine_b->create_session(spec);
    auto first_b = std::get<ear::QuestionBundle>(engine_b->next_question(session_b));
    engine_b->submit_result(session_b, make_report(first_b));
    auto second_b = std::get<ear::QuestionBundle>(engine_b->next_question(session_b));

    suite.require(digest(second_a) == digest(second_b),
                  "RNG advanced outside next_question (assist/submit side effects)");
  }

  {
    auto engine = ear::make_engine();
   auto spec = make_spec("chord", "eager", 999, 1);
   auto session = engine->create_session(spec);
    auto bundle = std::get<ear::QuestionBundle>(engine->next_question(session));
    auto json_bundle = ear::bridge::to_json(bundle);
    auto round_trip_bundle = ear::bridge::question_bundle_from_json(json_bundle);
    auto json_bundle_repeat = ear::bridge::to_json(round_trip_bundle);
    suite.require(json_bundle == json_bundle_repeat, "QuestionsBundle JSON round-trip failed");

    auto report = make_report(bundle);
    auto json_report = ear::bridge::to_json(report);
    auto round_trip_report = ear::bridge::result_report_from_json(json_report);
    suite.require(ear::bridge::to_json(round_trip_report) == json_report,
                  "ResultReport JSON round-trip failed");

    engine->submit_result(session, report);
    auto summary_variant = engine->next_question(session);
    const auto* summary_ptr = std::get_if<ear::SessionSummary>(&summary_variant);
    if (summary_ptr != nullptr) {
      auto json_summary = ear::bridge::to_json(*summary_ptr);
      auto round_summary = ear::bridge::session_summary_from_json(json_summary);
      suite.require(ear::bridge::to_json(round_summary) == json_summary,
                    "SessionSummary JSON round-trip failed");
    }
  }

  {
    auto engine = ear::make_engine();
    auto spec = make_spec("note", "manual", 31415, 0);
    spec.mode = ear::SessionMode::LevelInspector;
    spec.level_inspect = true;
    spec.inspect_level = 1;
    spec.inspect_tier = 0;
    spec.n_questions = 0;
    auto session = engine->create_session(spec);
    auto overview = engine->level_catalog_overview(session);
    suite.require(overview.find("Level 1") != std::string::npos,
                  "Level inspector overview should mention level 1");
    auto initial_variant = engine->next_question(session);
    auto* initial_bundle = std::get_if<ear::QuestionBundle>(&initial_variant);
    suite.require(initial_bundle != nullptr, "Level inspector should return question bundle");
    suite.require(initial_bundle->question_id.rfind("li-", 0) == 0,
                  "Level inspector question id must start with li-");
    auto follow_variant = engine->submit_result(session, make_report(*initial_bundle));
    suite.require(std::holds_alternative<ear::QuestionBundle>(follow_variant) ||
                      std::holds_alternative<ear::SessionSummary>(follow_variant),
                  "Level inspector submit should produce bundle or summary");
    engine->set_level(session, 1, 0);
    auto after_reset = engine->next_question(session);
    suite.require(std::holds_alternative<ear::QuestionBundle>(after_reset),
                  "Level inspector should provide question after reset");
  }

  {
    ear::scoring::MelodyScoringConfig cfg;
    cfg.tempo_min = 60.0;
    cfg.tempo_max = 180.0;
    ear::scoring::MelodyScorer scorer{cfg};

    ear::QuestionBundleV0 question;
    question.question_id = "q-test";
    nlohmann::json payload = nlohmann::json::object();
    nlohmann::json midi = nlohmann::json::array();
    midi.push_back(60);
    midi.push_back(62);
    midi.push_back(64);
    midi.push_back(65);
    midi.push_back(67);
    payload["midi"] = midi;

    nlohmann::json degrees = nlohmann::json::array();
    degrees.push_back(0);
    degrees.push_back(1);
    degrees.push_back(2);
    degrees.push_back(3);
    degrees.push_back(4);
    payload["degrees"] = degrees;
    question.question = ear::TypedPayload{"melody", payload};
    question.correct_answer = ear::TypedPayload{"melody_notes", payload};
    ear::PromptPlan prompt;
    prompt.modality = "midi";
    prompt.tempo_bpm = 120;
    prompt.count_in = true;
    prompt.notes = {
        ear::Note{60, 400, std::nullopt, std::nullopt},
        ear::Note{62, 400, std::nullopt, std::nullopt},
        ear::Note{64, 400, std::nullopt, std::nullopt},
        ear::Note{65, 400, std::nullopt, std::nullopt},
        ear::Note{67, 400, std::nullopt, std::nullopt},
    };
    question.prompt = prompt;
    question.ui_hints = nlohmann::json::object();

    ear::ResultReport report;
    report.question_id = question.question_id;
    report.correct = true;
    report.metrics.rt_ms = 2000;
    report.metrics.attempts = 1;
    report.metrics.assists_used = {};
    report.metrics.first_input_rt_ms = 1500;

    const double sn = (5.0 - 1.0) / (cfg.max_notes - 1.0);
    const double st = (120.0 - cfg.tempo_min.value()) / (cfg.tempo_max.value() - cfg.tempo_min.value());
    const double qd = 0.5 * sn + 0.5 * st;
    const double sr = -0.5 * ((2.0 - cfg.response_time_min) /
                              (cfg.response_time_max - cfg.response_time_min)) + 1.0;
    const double expected_score = cfg.weight_response_vs_qd * sr +
                                  (1.0 - cfg.weight_response_vs_qd) * qd;

    const double score = scorer.score_question(question, report);
    suite.require(std::abs(score - expected_score) < 1e-6,
                  "score_question should combine Sr and QD for correct answers");

    report.correct = false;
    suite.require(std::abs(scorer.score_question(question, report)) < 1e-9,
                  "score_question should return zero for incorrect answers");

    const double target_fitness = cfg.target_success_rate * expected_score;
    auto menu = scorer.menu_for_fitness(target_fitness, 5);
    suite.require(!menu.empty(), "menu_for_fitness should produce entries");

    bool matched = false;
    for (const auto& entry : menu) {
      if (entry.note_count == 5 && entry.tempo_bpm.has_value()) {
        if (std::abs(entry.tempo_bpm.value() - 120.0) < 1e-3) {
          matched = true;
          suite.require(std::abs(entry.expected_score - target_fitness) < 0.05,
                        "menu entry expected score should be close to target");
          break;
        }
      }
    }
    suite.require(matched, "menu_for_fitness should include a matching drill option");
  }

  {
    auto engine = ear::make_engine();
    auto spec = make_spec("melody", "adaptive", 4242, 2);
    spec.adaptive = true;
    spec.track_levels = {11, 0, 0};
    spec.sampler_params = nlohmann::json::object();
    nlohmann::json drills = nlohmann::json::array();
    drills.push_back("melody");
    drills.push_back("note");
    spec.sampler_params["drills"] = drills;

    auto session = engine->create_session(spec);

    auto first_variant = engine->next_question(session);
    auto* first_bundle = std::get_if<ear::QuestionBundle>(&first_variant);
    suite.require(first_bundle != nullptr, "Adaptive session should yield first question");

    auto first_report = make_report(*first_bundle);
    auto first_response = engine->submit_result(session, first_report);
    suite.require(std::holds_alternative<ear::QuestionBundle>(first_response),
                  "Adaptive submit should return bundle until session ends");

    auto second_variant = engine->next_question(session);
    auto* second_bundle = std::get_if<ear::QuestionBundle>(&second_variant);
    suite.require(second_bundle != nullptr, "Adaptive session should yield second question");

    auto second_report = make_report(*second_bundle);
    auto second_response = engine->submit_result(session, second_report);
    auto* summary = std::get_if<ear::SessionSummary>(&second_response);
    suite.require(summary != nullptr,
                  "Adaptive session should provide summary after configured questions");
    suite.require(summary->totals.contains("correct"),
                  "Summary totals should contain correctness info");
  }

  {
    auto engine = ear::make_engine();

    auto spec = make_spec("melody", "adaptive", 9999, 6);
    spec.adaptive = true;
    spec.track_levels = {11, 0, 0};
    spec.sampler_params = nlohmann::json::object();
    nlohmann::json drills = nlohmann::json::array();
    drills.push_back("melody");
    drills.push_back("note");
    spec.sampler_params["drills"] = drills;
    nlohmann::json balanced = nlohmann::json::object();
    balanced["melody"] = 1.0;
    balanced["note"] = 1.0;
    spec.sampler_params["weights"] = balanced;

    auto session = engine->create_session(spec);

    for (int i = 0; i < spec.n_questions; ++i) {
      auto next = engine->next_question(session);
      auto* bundle = std::get_if<ear::QuestionBundle>(&next);
      suite.require(bundle != nullptr, "Unbiased adaptive session should deliver bundle");
      auto report = make_report(*bundle);
      engine->submit_result(session, report);
    }

    auto debug_json = engine->debug_state(session);
    suite.require(debug_json.contains("drill_counts"),
                  "Debug state should expose drill counts");
    const auto& counts = debug_json["drill_counts"].get_object();
    const auto melody_it = counts.find("melody");
    const auto note_it = counts.find("note");
    const auto melody_count =
        melody_it != counts.end()
            ? static_cast<std::size_t>(melody_it->second.get<int>())
            : 0U;
    const auto note_count =
        note_it != counts.end()
            ? static_cast<std::size_t>(note_it->second.get<int>())
            : 0U;
    suite.require(melody_count > 0 && note_count > 0,
                  "Balanced weights should sample multiple drill kinds");
    suite.require(melody_count + note_count ==
                      static_cast<std::size_t>(spec.n_questions),
                  "Drill counts should total configured questions");
  }

  {
    auto engine = ear::make_engine();

    auto spec = make_spec("melody", "adaptive", 7777, 4);
    spec.adaptive = true;
    spec.track_levels = {11, 0, 0};
    spec.sampler_params = nlohmann::json::object();
    nlohmann::json drills = nlohmann::json::array();
    drills.push_back("melody");
    drills.push_back("note");
    spec.sampler_params["drills"] = drills;
    nlohmann::json biased = nlohmann::json::object();
    biased["melody"] = 5.0;
    biased["note"] = 0.0;
    spec.sampler_params["weights"] = biased;

    auto session = engine->create_session(spec);

    for (int i = 0; i < spec.n_questions; ++i) {
      auto next = engine->next_question(session);
      auto* bundle = std::get_if<ear::QuestionBundle>(&next);
      suite.require(bundle != nullptr, "Biased adaptive session should deliver bundle");
      auto report = make_report(*bundle);
      engine->submit_result(session, report);
    }

    auto debug_json = engine->debug_state(session);
    const auto& counts = debug_json["drill_counts"].get_object();
    const auto melody_it = counts.find("melody");
    const auto melody_count =
        melody_it != counts.end()
            ? static_cast<std::size_t>(melody_it->second.get<int>())
            : 0U;
    auto it = counts.find("note");
    suite.require(it == counts.end() || it->second.get<int>() == 0,
                  "Debug counts should reflect zero-weight drill usage");
    suite.require(melody_count == static_cast<std::size_t>(spec.n_questions),
                  "Biased weights should allocate all questions to the weighted drill");
  }

  if (!suite.ok) {
    std::cerr << "Ear Trainer core tests FAILED" << std::endl;
    return 1;
  }

  std::cout << "Ear Trainer core tests passed" << std::endl;
  return 0;
}
