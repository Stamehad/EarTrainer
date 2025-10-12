#pragma once

#include "../include/ear/types.hpp"

namespace ear::assistance {

AssistBundle make_assist(const QuestionBundle& question, const std::string& kind);

} // namespace ear::assistance
