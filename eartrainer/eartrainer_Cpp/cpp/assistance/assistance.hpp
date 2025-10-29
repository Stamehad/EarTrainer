#pragma once

#include "../include/ear/types.hpp"
#include "../include/ear/midi_clip.hpp"

#include <string>
#include <vector>

namespace ear::assistance {

// Question-scoped assists (currently unused but kept for compatibility)
AssistBundle make_assist(const QuestionBundle& question, const std::string& kind);

// Session-scoped orientation assists
std::vector<std::string> session_assist_kinds();
AssistBundle make_session_assist(const SessionSpec& spec, const std::string& kind);
MidiClip session_assist_clip(const SessionSpec& spec, const std::string& kind);
MidiClip orientation_clip(const SessionSpec& spec);

} // namespace ear::assistance
