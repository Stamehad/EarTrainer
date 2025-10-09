#include "ear/adaptive_drills.hpp"

#include "ear/adaptive_catalog.hpp"
#include "ear/drill_factory.hpp"
#include "rng.hpp"

#include <stdexcept>
#include <mutex>
#include <sstream>
#include <utility>

namespace ear {

namespace {

void ensure_factory_registered() {
  static std::once_flag flag;
  std::call_once(flag, []() {
    auto& factory = DrillFactory::instance();
    register_builtin_drills(factory);
  });
}

} // namespace

AdaptiveDrills::AdaptiveDrills(std::string catalog_path, std::uint64_t seed)
    : catalog_path_(std::move(catalog_path)),
      master_rng_(seed == 0 ? 1 : seed),
      factory_(DrillFactory::instance()) {
  ensure_factory_registered();
}

void AdaptiveDrills::set_bout(int level) {
  auto specs = adaptive::load_level_catalog(catalog_path_, level);
  initialize_bout(level, specs);
}

void AdaptiveDrills::set_bout_from_json(int level, const nlohmann::json& document) {
  auto specs = DrillSpec::load_json(document);
  auto filtered = DrillSpec::filter_by_level(specs, level);
  if (filtered.empty()) {
    throw std::runtime_error("No adaptive drills configured for level " + std::to_string(level));
  }
  initialize_bout(level, filtered);
}

void AdaptiveDrills::initialize_bout(int level, const std::vector<DrillSpec>& specs) {
  slots_.clear();
  question_counter_ = 0;
  current_level_ = level;
  pick_counts_.clear();
  last_pick_.reset();

  if (specs.empty()) {
    throw std::runtime_error("Adaptive bout has no drills configured");
  }

  std::uint64_t seed = master_rng_;
  for (const auto& spec : specs) {
    auto assignment = factory_.create(spec);
    Slot slot;
    slot.id = std::move(assignment.id);
    slot.family = std::move(assignment.family);
    slot.spec = assignment.spec;
    slot.module = std::move(assignment.module);
    slot.rng_state = advance_rng(seed);
    slots_.push_back(std::move(slot));
    pick_counts_.push_back(0);
  }

  master_rng_ = seed;
  if (slots_.empty()) {
    throw std::runtime_error("Adaptive bout initialization produced zero drills");
  }
}

QuestionBundle AdaptiveDrills::next() {
  if (slots_.empty()) {
    throw std::runtime_error("AdaptiveDrills::next called before set_bout or with empty bout");
  }

  const int max_index = static_cast<int>(slots_.size()) - 1;
  int pick = rand_int(master_rng_, 0, max_index);
  auto& slot = slots_[static_cast<std::size_t>(pick)];
  pick_counts_[static_cast<std::size_t>(pick)] += 1;
  last_pick_ = static_cast<std::size_t>(pick);
  auto output = slot.module->next_question(slot.rng_state);

  QuestionBundle bundle;
  bundle.question_id = make_question_id();
  bundle.question = std::move(output.question);
  bundle.correct_answer = std::move(output.correct_answer);
  bundle.prompt = std::move(output.prompt);
  bundle.ui_hints = std::move(output.ui_hints);
  return bundle;
}

std::string AdaptiveDrills::make_question_id() {
  std::ostringstream oss;
  oss << "ad-";
  oss.width(3);
  oss.fill('0');
  oss << ++question_counter_;
  return oss.str();
}

nlohmann::json AdaptiveDrills::diagnostic() const {
  nlohmann::json info = nlohmann::json::object();
  info["catalog_path"] = catalog_path_;
  info["level"] = current_level_;
  info["slots"] = static_cast<int>(slots_.size());
  info["questions_emitted"] = static_cast<int>(question_counter_);
  if (last_pick_.has_value()) {
    info["last_pick"] = static_cast<int>(*last_pick_);
  } else {
    info["last_pick"] = nullptr;
  }
  nlohmann::json ids = nlohmann::json::array();
  nlohmann::json families = nlohmann::json::array();
  nlohmann::json counts = nlohmann::json::array();
  for (std::size_t i = 0; i < slots_.size(); ++i) {
    ids.push_back(slots_[i].id);
    families.push_back(slots_[i].family);
    counts.push_back(static_cast<int>(i < pick_counts_.size() ? pick_counts_[i] : 0));
  }
  info["ids"] = ids;
  info["families"] = families;
  info["pick_counts"] = counts;
  return info;
}

} // namespace ear
