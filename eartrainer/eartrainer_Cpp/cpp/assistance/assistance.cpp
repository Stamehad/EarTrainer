#include "assistance.hpp"
#include "../include/ear/question_bundle_v2.hpp"

#include <algorithm>

namespace ear::assistance {

AssistBundle make_assist(const QuestionBundle& question, const std::string& kind) {
  AssistBundle bundle;
  bundle.question_id = question.question_id;
  bundle.kind = kind;
  bundle.prompt = std::nullopt;
  bundle.ui_delta = nlohmann::json::object();

  // if (kind == "Replay") {
  //   if (question.prompt.has_value()) {
  //     bundle.prompt = question.prompt;
  //   }
  //   bundle.ui_delta["message"] = "Replay the question";
  // } else if (kind == "GuideTone") {
  //   if (question.prompt.has_value() && !question.prompt->notes.empty()) {
  //     PromptPlan plan;
  //     plan.modality = question.prompt->modality;
  //     plan.count_in = false;
  //     plan.tempo_bpm = question.prompt->tempo_bpm;
  //     plan.notes.push_back(question.prompt->notes.front());
  //     bundle.prompt = plan;
  //   }
  //   bundle.ui_delta["message"] = "Guide tone provided";
  // } else if (kind == "TempoDown") {
  //   if (question.prompt.has_value()) {
  //     PromptPlan plan = *question.prompt;
  //     if (plan.tempo_bpm.has_value()) {
  //       plan.tempo_bpm = std::max(40, plan.tempo_bpm.value() - 20);
  //     }
  //     bundle.prompt = plan;
  //   }
  //   bundle.ui_delta["message"] = "Tempo reduced";
  // } else if (kind == "PathwayHint") {
  //   bundle.ui_delta["message"] = "Focus on solfege degrees";
  // } else {
  //   bundle.ui_delta["message"] = "Assist not recognized";
  // }

  return bundle;
}

} // namespace ear::assistance
