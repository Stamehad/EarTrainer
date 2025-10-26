#pragma once

#include "../include/ear/types.hpp"

namespace ear::assistance {

AssistBundle make_assist(const QuestionsBundle& question, const std::string& kind);

} // namespace ear::assistance
