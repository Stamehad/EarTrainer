#include "scoring.hpp"

namespace ear::scoring {

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

