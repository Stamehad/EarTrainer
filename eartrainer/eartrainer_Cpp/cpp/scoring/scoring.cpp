#include "scoring.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace ear::scoring {

void MelodyScoringConfig::validate() const {
  if (max_notes <= 1) {
    throw std::invalid_argument("max_notes must be greater than 1");
  }
  if (response_time_min >= response_time_max) {
    throw std::invalid_argument("response_time_min must be less than response_time_max");
  }
  if (weight_notes_vs_tempo < 0.0 || weight_notes_vs_tempo > 1.0) {
    throw std::invalid_argument("weight_notes_vs_tempo must be within [0, 1]");
  }
  if (weight_response_vs_qd < 0.0 || weight_response_vs_qd > 1.0) {
    throw std::invalid_argument("weight_response_vs_qd must be within [0, 1]");
  }
  if (target_success_rate < 0.0 || target_success_rate > 1.0) {
    throw std::invalid_argument("target_success_rate must be within [0, 1]");
  }
  if (tempo_min.has_value() != tempo_max.has_value()) {
    throw std::invalid_argument("tempo_min and tempo_max must both be set or both unset");
  }
  if (has_tempo_bounds() && tempo_min.value() >= tempo_max.value()) {
    throw std::invalid_argument("tempo_min must be less than tempo_max");
  }
}

MelodyScorer::MelodyScorer() {
  config_.validate();
}

MelodyScorer::MelodyScorer(MelodyScoringConfig config) : config_(std::move(config)) {
  config_.validate();
}

double MelodyScorer::score_question(const QuestionBundle& question, const ResultReport& report,
                                    bool include_response_score) const {
  const int note_count = extract_note_count(question);
  const auto tempo = extract_tempo(question);
  const double rt_seconds = ms_to_seconds(report.metrics.rt_ms);

  const double qd = question_difficulty(note_count, tempo);
  double base = qd;
  if (include_response_score && config_.weight_response_vs_qd > 0.0) {
    const double sr = response_score(rt_seconds);
    base = config_.weight_response_vs_qd * sr + (1.0 - config_.weight_response_vs_qd) * qd;
  }

  if (!report.correct) {
    return 0.0;
  }
  return base;
}

std::vector<MelodyMenuEntry> MelodyScorer::menu_for_fitness(double fitness, std::size_t max_entries,
                                                            bool include_response_score) const {
  std::vector<MelodyMenuEntry> menu;
  if (max_entries == 0) {
    return menu;
  }

  const double target = clip01(fitness);
  if (config_.target_success_rate <= 0.0) {
    return menu;
  }

  struct Candidate {
    MelodyMenuEntry entry;
    double diff;
  };

  std::vector<Candidate> candidates;
  const auto tempos = candidate_tempos();
  const auto responses = candidate_response_times();

  for (int note_count = 1; note_count <= config_.max_notes; ++note_count) {
    for (const auto& tempo : tempos) {
      for (double response_time : responses) {
        const double expected = expected_score(note_count, response_time, tempo, include_response_score);
        const double diff = std::abs(expected - target);
        MelodyMenuEntry entry{note_count,
                              tempo,
                              response_time,
                              question_difficulty(note_count, tempo),
                              expected};
        candidates.push_back({entry, diff});
      }
    }
  }

  std::sort(candidates.begin(), candidates.end(),
            [](const Candidate& a, const Candidate& b) { return a.diff < b.diff; });

  for (const auto& candidate : candidates) {
    // Avoid duplicates that may occur when tempo bounds are missing.
    const bool duplicate = std::any_of(menu.begin(), menu.end(), [&](const MelodyMenuEntry& entry) {
      const bool tempo_equal = entry.tempo_bpm.has_value() == candidate.entry.tempo_bpm.has_value() &&
                               (!entry.tempo_bpm.has_value() ||
                                std::abs(entry.tempo_bpm.value() - candidate.entry.tempo_bpm.value()) < 1e-9);
      return entry.note_count == candidate.entry.note_count &&
             tempo_equal &&
             std::abs(entry.target_response_time_sec - candidate.entry.target_response_time_sec) < 1e-9;
    });
    if (duplicate) {
      continue;
    }
    menu.push_back(candidate.entry);
    if (menu.size() >= max_entries) {
      break;
    }
  }

  return menu;
}

double MelodyScorer::notes_score(int note_count) const {
  if (note_count <= 1) {
    return 0.0;
  }
  const double numerator = static_cast<double>(note_count - 1);
  const double denominator = static_cast<double>(config_.max_notes - 1);
  if (denominator <= 0.0) {
    return 0.0;
  }
  return clip01(numerator / denominator);
}

double MelodyScorer::tempo_score(std::optional<double> tempo) const {
  if (!tempo.has_value()) {
    return 0.0;
  }
  if (!config_.has_tempo_bounds()) {
    throw std::logic_error("tempo bounds must be configured to compute tempo score");
  }
  const double numerator = tempo.value() - config_.tempo_min.value();
  const double denominator = config_.tempo_max.value() - config_.tempo_min.value();
  if (denominator <= 0.0) {
    return 0.0;
  }
  return clip01(numerator / denominator);
}

double MelodyScorer::response_score(double response_time) const {
  const double denominator = config_.response_time_max - config_.response_time_min;
  if (denominator <= 0.0) {
    return 0.0;
  }
  const double numerator = response_time - config_.response_time_min;
  const double raw = -0.5 * (numerator / denominator) + 1.0;
  return clip01(raw);
}

double MelodyScorer::question_difficulty(int note_count, std::optional<double> tempo) const {
  const double w1 = config_.weight_notes_vs_tempo;
  const double sn = notes_score(note_count);
  const double st = tempo.has_value() ? tempo_score(tempo) : 0.0;
  return w1 * sn + (1.0 - w1) * st;
}

double MelodyScorer::expected_score(int note_count, double response_time,
                                    std::optional<double> tempo,
                                    bool include_response_score) const {
  const double qd = question_difficulty(note_count, tempo);
  const double p_star = config_.target_success_rate;
  if (!include_response_score) {
    return p_star * qd;
  }
  const double w2 = config_.weight_response_vs_qd;
  const double sr = response_score(response_time);
  return p_star * (w2 * sr + (1.0 - w2) * qd);
}

int MelodyScorer::extract_note_count(const QuestionBundle& question) const {
  if (question.prompt.has_value()) {
    const auto& prompt = question.prompt.value();
    if (!prompt.notes.empty()) {
      return static_cast<int>(prompt.notes.size());
    }
  }

  const auto& payload = question.question.payload;
  if (payload.is_object()) {
    if (payload.contains("midi")) {
      const auto& midi = payload["midi"];
      if (midi.is_array()) {
        return static_cast<int>(midi.size());
      }
    }
    if (payload.contains("degrees")) {
      const auto& degrees = payload["degrees"];
      if (degrees.is_array()) {
        return static_cast<int>(degrees.size());
      }
    }
  }

  return 1;
}

std::optional<double> MelodyScorer::extract_tempo(const QuestionBundle& question) const {
  if (question.prompt.has_value()) {
    const auto& prompt = question.prompt.value();
    if (prompt.tempo_bpm.has_value()) {
      return static_cast<double>(prompt.tempo_bpm.value());
    }
  }
  return std::nullopt;
}

double MelodyScorer::ms_to_seconds(int milliseconds) const {
  return static_cast<double>(milliseconds) / 1000.0;
}

std::vector<std::optional<double>> MelodyScorer::candidate_tempos() const {
  if (!config_.has_tempo_bounds()) {
    return {std::nullopt};
  }
  const double min = config_.tempo_min.value();
  const double max = config_.tempo_max.value();
  if (min >= max) {
    return {min};
  }

  std::vector<std::optional<double>> values;
  constexpr int steps = 4;
  const double step = (max - min) / static_cast<double>(steps);
  for (int i = 0; i <= steps; ++i) {
    values.push_back(min + step * static_cast<double>(i));
  }
  return values;
}

std::vector<double> MelodyScorer::candidate_response_times() const {
  std::vector<double> values;
  const double min = config_.response_time_min;
  const double max = config_.response_time_max;
  if (min >= max) {
    values.push_back(min);
    return values;
  }
  constexpr int steps = 4;
  const double step = (max - min) / static_cast<double>(steps);
  for (int i = 0; i <= steps; ++i) {
    values.push_back(min + step * static_cast<double>(i));
  }
  return values;
}

double score_question(const ResultReport& report) {
  double score = report.correct ? 1.0 : 0.0;
  const auto& metrics = report.metrics;
  double latency_penalty = 0.0;
  if (metrics.rt_ms > 1500) {
    latency_penalty = static_cast<double>(metrics.rt_ms - 1500) / 5000.0;
  }
  double assist_penalty = 0.0;
  for (const auto& entry : metrics.assists_used) {
    assist_penalty += static_cast<double>(entry.second) * 0.05;
  }
  score -= (latency_penalty + assist_penalty);
  if (score < 0.0) {
    score = 0.0;
  }
  return score;
}

double aggregate_accuracy(const std::vector<ResultReport>& results) {
  if (results.empty()) {
    return 0.0;
  }
  int correct = 0;
  for (const auto& r : results) {
    if (r.correct) {
      ++correct;
    }
  }
  return static_cast<double>(correct) / static_cast<double>(results.size());
}

double average_response_time(const std::vector<ResultReport>& results) {
  if (results.empty()) {
    return 0.0;
  }
  double total = 0.0;
  for (const auto& r : results) {
    total += static_cast<double>(r.metrics.rt_ms);
  }
  return total / static_cast<double>(results.size());
}

} // namespace ear::scoring
