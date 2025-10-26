#pragma once

#include "../include/ear/session_engine.hpp"

namespace ear::bridge {

nlohmann::json to_json(const SessionSpec& spec);
SessionSpec session_spec_from_json(const nlohmann::json& json_spec);

nlohmann::json to_json(const QuestionsBundle& bundle);
QuestionsBundle question_bundle_from_json(const nlohmann::json& json_bundle);

nlohmann::json to_json(const AssistBundle& bundle);
AssistBundle assist_bundle_from_json(const nlohmann::json& json_bundle);

nlohmann::json to_json(const ResultReport& report);
ResultReport result_report_from_json(const nlohmann::json& json_report);

nlohmann::json to_json(const SessionSummary& summary);
SessionSummary session_summary_from_json(const nlohmann::json& json_summary);

nlohmann::json to_json(const MemoryPackage& package);

nlohmann::json to_json(const PromptPlan& plan);
PromptPlan prompt_plan_from_json(const nlohmann::json& json_plan);

nlohmann::json to_json(const LevelCatalogEntry& entry);
LevelCatalogEntry level_catalog_entry_from_json(const nlohmann::json& json_entry);

} // namespace ear::bridge
