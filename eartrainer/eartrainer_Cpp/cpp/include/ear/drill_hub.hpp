#pragma once

#include "../../drills/drill.hpp"
#include "types.hpp"

#include "../nlohmann/json.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ear {

class DrillHub {
public:
  struct Entry {
    std::string drill_kind;
    std::unique_ptr<DrillModule> module;
    DrillSpec spec;
    double weight = 1.0;
  };

  struct Selection {
    std::string drill_kind;
    QuestionBundle bundle;
    DrillSpec spec;
  };

  DrillHub() = default;
  DrillHub(std::vector<Entry> entries, std::uint64_t seed);

  Selection next();

  nlohmann::json debug_state() const;
  const std::string& last_selected_kind() const { return last_selected_kind_; }

  void set_weights(const std::vector<double>& weights);

  void set_weight_bias(const std::unordered_map<std::string, double>& bias);

  void reset_uniform();

  bool empty() const { return nodes_.empty(); }
  std::size_t size() const { return nodes_.size(); }

private:
  struct Node {
    std::string drill_kind;
    std::unique_ptr<DrillModule> module;
    DrillSpec spec;
    double weight = 1.0;
    double cumulative = 0.0;
    std::uint64_t module_rng_state = 0;
  };

  void recompute_cumulative();

  std::vector<Node> nodes_;
  double total_weight_ = 0.0;
  std::uint64_t hub_rng_state_ = 0;
  std::string last_selected_kind_;
};

} // namespace ear
