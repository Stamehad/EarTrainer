#pragma once

#include "ear/types.hpp"

#include <optional>
#include <vector>

#include <nlohmann/json.hpp>

namespace ear::scoring {

inline double clip01(double value) {
  if (value < 0.0) {
    return 0.0;
  }
  if (value > 1.0) {
    return 1.0;
  }
  return value;
}

struct MelodyScoringConfig {
  int max_notes = 8;
  std::optional<double> tempo_min{};
  std::optional<double> tempo_max{};
  double response_time_min = 1.0;
  double response_time_max = 5.0;
  double weight_notes_vs_tempo = 0.5;
  double weight_response_vs_qd = 0.5;
  double target_success_rate = 0.85;

  bool has_tempo_bounds() const { return tempo_min.has_value() && tempo_max.has_value(); }

  void validate() const;
};

struct MelodyMenuEntry {
  int note_count;
  std::optional<double> tempo_bpm;
  double target_response_time_sec;
  double question_difficulty;
  double expected_score;
};

class MelodyScorer {
public:
  MelodyScorer();
  explicit MelodyScorer(MelodyScoringConfig config);

  const MelodyScoringConfig& config() const noexcept { return config_; }

  double score_question(const QuestionBundle& question, const ResultReport& report,
                        bool include_response_score = true) const;

  std::vector<MelodyMenuEntry> menu_for_fitness(double fitness, std::size_t max_entries = 5,
                                                bool include_response_score = true) const;

private:
  double notes_score(int note_count) const;
  double tempo_score(std::optional<double> tempo) const;
  double response_score(double response_time) const;
  double question_difficulty(int note_count, std::optional<double> tempo = std::nullopt) const;
  double expected_score(int note_count, double response_time,
                        std::optional<double> tempo = std::nullopt,
                        bool include_response_score = false) const;

  int extract_note_count(const QuestionBundle& question) const;
  std::optional<double> extract_tempo(const QuestionBundle& question) const;
  double ms_to_seconds(int milliseconds) const;
  std::vector<std::optional<double>> candidate_tempos() const;
  std::vector<double> candidate_response_times() const;

  MelodyScoringConfig config_{};
};

double score_question(const ResultReport& report);

double aggregate_accuracy(const std::vector<ResultReport>& results);

double average_response_time(const std::vector<ResultReport>& results);

} // namespace ear::scoring
