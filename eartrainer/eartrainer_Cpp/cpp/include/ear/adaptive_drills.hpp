#pragma once

#include "ear/drill_factory.hpp"
#include "ear/types.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <optional>

#include <nlohmann/json.hpp>

namespace ear {

class AdaptiveDrills {
public:
  explicit AdaptiveDrills(std::string catalog_path = "resources/adaptive_levels.yml",
                          std::uint64_t seed = 1);

  void set_bout(int level);
  QuestionBundle next();
  nlohmann::json diagnostic() const;

  bool empty() const { return slots_.empty(); }
  std::size_t size() const { return slots_.size(); }

private:
  struct Slot {
    std::string id;
    std::string family;
    SessionSpec spec;
    std::unique_ptr<DrillModule> module;
    std::uint64_t rng_state = 0;
  };

  std::string make_question_id();

  std::string catalog_path_;
  std::uint64_t master_rng_;
  std::size_t question_counter_ = 0;
  int current_level_ = 0;
  DrillFactory& factory_;
  std::vector<Slot> slots_;
  std::vector<std::size_t> pick_counts_;
  std::optional<std::size_t> last_pick_;
};

} // namespace ear
