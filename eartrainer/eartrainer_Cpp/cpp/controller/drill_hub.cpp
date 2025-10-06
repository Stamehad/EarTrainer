#include "ear/drill_hub.hpp"

#include "../src/rng.hpp"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace ear {

namespace {
constexpr double kEpsilon = 1e-9;
}

DrillHub::DrillHub(std::vector<Entry> entries, std::uint64_t seed)
    : hub_rng_state_(seed == 0 ? 1 : seed) {
  nodes_.reserve(entries.size());
  std::uint64_t module_seed = hub_rng_state_;
  for (auto& entry : entries) {
    if (!entry.module) {
      throw std::invalid_argument("DrillHub entry requires a drill module");
    }
    Node node;
    node.drill_kind = std::move(entry.drill_kind);
    node.module = std::move(entry.module);
    node.spec = std::move(entry.spec);
    node.weight = entry.weight;
    node.module_rng_state = advance_rng(module_seed);
    nodes_.push_back(std::move(node));
  }

  recompute_cumulative();
}

DrillHub::Selection DrillHub::next() {
  if (nodes_.empty()) {
    throw std::runtime_error("DrillHub has no drills configured");
  }

  if (total_weight_ <= kEpsilon) {
    throw std::runtime_error("DrillHub total weight is zero");
  }

  double pick = rand_unit(hub_rng_state_) * total_weight_;
  auto it = std::find_if(nodes_.begin(), nodes_.end(), [pick](const Node& node) {
    return pick <= node.cumulative;
  });
  if (it == nodes_.end()) {
    it = std::prev(nodes_.end());
  }
  auto& node = *it;

  auto output = node.module->next_question(node.spec, node.module_rng_state);
  DrillHub::Selection selection;
  selection.drill_kind = node.drill_kind;
  selection.output = std::move(output);
  selection.spec = node.spec;
  last_selected_kind_ = node.drill_kind;
  return selection;
}

void DrillHub::set_weights(const std::vector<double>& weights) {
  if (weights.size() != nodes_.size()) {
    throw std::invalid_argument("DrillHub::set_weights size mismatch");
  }
  for (std::size_t i = 0; i < nodes_.size(); ++i) {
    nodes_[i].weight = weights[i];
  }
  recompute_cumulative();
}

void DrillHub::set_weight_bias(const std::unordered_map<std::string, double>& bias) {
  for (auto& node : nodes_) {
    auto it = bias.find(node.drill_kind);
    if (it != bias.end()) {
      node.weight = it->second;
    }
  }
  recompute_cumulative();
}

void DrillHub::reset_uniform() {
  if (nodes_.empty()) {
    total_weight_ = 0.0;
    return;
  }
  const double uniform = 1.0;
  for (auto& node : nodes_) {
    node.weight = uniform;
  }
  recompute_cumulative();
}

void DrillHub::recompute_cumulative() {
  total_weight_ = 0.0;
  for (auto& node : nodes_) {
    if (node.weight < 0.0) {
      node.weight = 0.0;
    }
    total_weight_ += node.weight;
  }

  if (total_weight_ <= kEpsilon) {
    if (!nodes_.empty()) {
      const double uniform = 1.0;
      total_weight_ = uniform * static_cast<double>(nodes_.size());
      double acc = 0.0;
      for (auto& node : nodes_) {
        node.weight = uniform;
        acc += node.weight;
        node.cumulative = acc;
      }
    }
    return;
  }

  double acc = 0.0;
  for (auto& node : nodes_) {
    acc += node.weight;
    node.cumulative = acc;
  }
}

nlohmann::json DrillHub::debug_state() const {
  nlohmann::json info = nlohmann::json::object();
  info["total_weight"] = total_weight_;
  info["last_selected"] = last_selected_kind_;
  nlohmann::json entries = nlohmann::json::array();
  for (const auto& node : nodes_) {
    nlohmann::json entry = nlohmann::json::object();
    entry["drill_kind"] = node.drill_kind;
    entry["weight"] = node.weight;
    entry["cumulative"] = node.cumulative;
    entries.push_back(entry);
  }
  info["entries"] = entries;
  info["size"] = static_cast<int>(nodes_.size());
  return info;
}

} // namespace ear
