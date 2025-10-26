#pragma once
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "drill_spec.hpp"
#include "question_bundle_v2.hpp"
#include "../../drills/drill.hpp"

namespace ear {

struct DrillAssignment {
  std::string id;
  std::string family;
  std::unique_ptr<DrillModule> module;
  DrillSpec spec;
};

class DrillFactory {
public:
  using Creator = std::function<std::unique_ptr<DrillModule>()>;

  static DrillFactory& instance();

  void register_family(const std::string& family, Creator create);

  std::unique_ptr<DrillModule> create_module(const std::string& family) const;

  DrillAssignment create(const DrillSpec& spec) const;

  std::vector<DrillAssignment> create_for_level(const std::vector<DrillSpec>& all, int level) const;

private:
  std::unordered_map<std::string, Creator> registry_;
};

void register_builtin_drills(DrillFactory& factory);

// void apply_prompt_rendering(const DrillSpec& spec, QuestionBundle& bundle);

} // namespace ear
