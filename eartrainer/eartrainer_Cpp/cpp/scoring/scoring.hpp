#pragma once

#include "ear/types.hpp"

#include <vector>

namespace ear::scoring {

double score_question(const ResultReport& report);

double aggregate_accuracy(const std::vector<ResultReport>& results);

double average_response_time(const std::vector<ResultReport>& results);

} // namespace ear::scoring
