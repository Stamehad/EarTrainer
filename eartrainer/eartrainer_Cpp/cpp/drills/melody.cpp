#include "melody.hpp"

#include "common.hpp"
#include "../src/rng.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <vector>

namespace ear {
namespace {

constexpr int kDefaultLength = 3;
constexpr int kMaxTries = 20;

static const int kSteps[] = {-7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7};
static const double kBaseWeights[] = {0.05, 0.06, 0.12, 0.18, 0.22, 0.45, 1.0, 0.1, 1.0, 0.45, 0.22, 0.18, 0.12, 0.06, 0.05};
static_assert(sizeof(kSteps) / sizeof(int) == sizeof(kBaseWeights) / sizeof(double),
              "interval weight tables must align");

struct MelodyState {
  int same_dir_run = 0;
  int unison_run = 0;
  std::optional<int> prev_step;
};

std::vector<int> base_degrees() {
  return {0, 1, 2, 3, 4, 5, 6};
}

std::vector<double> make_base_weights() {
  return std::vector<double>(std::begin(kBaseWeights), std::end(kBaseWeights));
}

void apply_musical_modifiers(std::vector<double>& weights, MelodyState state) {
  const std::size_t count = weights.size();
  auto weight_at = [&](int k) -> double& {
    for (std::size_t i = 0; i < count; ++i) {
      if (kSteps[i] == k) {
        return weights[i];
      }
    }
    return weights.front();
  };

  if (state.prev_step.has_value()) {
    const int prev = state.prev_step.value();
    if (std::abs(prev) >= 3) {
      const int sign = prev > 0 ? 1 : -1;
      for (std::size_t i = 0; i < count; ++i) {
        const int step = kSteps[i];
        if (step * sign > 0 && std::abs(step) >= 3) {
          weights[i] *= 0.35;
        }
      }
      const int recovery = (prev > 0) ? -1 : 1;
      weight_at(recovery) *= 1.6;
    }

    if (state.same_dir_run >= 4) {
      const int sign = prev > 0 ? 1 : -1;
      for (std::size_t i = 0; i < count; ++i) {
        const int step = kSteps[i];
        if (step * sign > 0) {
          weights[i] *= 0.3;
        }
      }
    }
  }

  if (state.unison_run > 0) {
    double factor = std::pow(0.35, static_cast<double>(state.unison_run));
    weight_at(0) *= factor;
    if (state.unison_run >= 2) {
      weight_at(0) *= 0.15;
    }
  }
}

std::vector<double> normalise(const std::vector<double>& weights) {
  std::vector<double> result(weights.size(), 0.0);
  double sum = 0.0;
  for (double w : weights) {
    sum += std::max(0.0, w);
  }
  if (sum <= 0.0) {
    const double uniform = 1.0 / static_cast<double>(weights.size());
    std::fill(result.begin(), result.end(), uniform);
  } else {
    for (std::size_t i = 0; i < weights.size(); ++i) {
      result[i] = std::max(0.0, weights[i]) / sum;
    }
  }
  return result;
}

int choose_step(std::uint64_t& rng_state, const std::vector<double>& probs) {
  double r = static_cast<double>(advance_rng(rng_state)) /
             static_cast<double>(std::numeric_limits<std::uint64_t>::max());
  double acc = 0.0;
  for (std::size_t i = 0; i < probs.size(); ++i) {
    acc += probs[i];
    if (r <= acc) {
      return kSteps[i];
    }
  }
  return kSteps[probs.size() - 1];
}

std::vector<int> generate_degrees(const SessionSpec& spec, std::uint64_t& rng_state) {
  int length = kDefaultLength;
  if (spec.sampler_params.contains("melody_length")) {
    length = std::max(1, spec.sampler_params["melody_length"].get<int>());
  }

  std::vector<int> all_degrees = base_degrees();
  int start_index = rand_int(rng_state, 0, static_cast<int>(all_degrees.size()) - 1);
  std::vector<int> sequence;
  sequence.reserve(static_cast<std::size_t>(length));
  sequence.push_back(all_degrees[static_cast<std::size_t>(start_index)]);

  MelodyState state;

  for (int i = 1; i < length; ++i) {
    auto weights = make_base_weights();
    apply_musical_modifiers(weights, state);
    auto probs = normalise(weights);
    int step = choose_step(rng_state, probs);

    int last_degree = sequence.back();
    int next_degree = ((last_degree + step) % 7 + 7) % 7;
    sequence.push_back(next_degree);

    if (step == 0) {
      state.unison_run += 1;
    } else {
      state.unison_run = 0;
    }

    if (state.prev_step.has_value() && state.prev_step.value() * step > 0) {
      state.same_dir_run += 1;
    } else {
      state.same_dir_run = 1;
    }

    state.prev_step = step;
  }

  return sequence;
}

std::vector<int> degrees_to_midi(const SessionSpec& spec, const std::vector<int>& degrees) {
  int tonic = drills::central_tonic_midi(spec.key);
  std::vector<int> base_midis;
  base_midis.reserve(degrees.size());
  for (int degree : degrees) {
    base_midis.push_back(tonic + drills::degree_to_offset(degree));
  }

  double target = static_cast<double>(tonic);
  bool has_range = spec.range_min < spec.range_max;
  if (has_range) {
    target = static_cast<double>(spec.range_min + spec.range_max) / 2.0;
  }

  std::vector<int> best_midis = base_midis;
  double best_dist = std::numeric_limits<double>::infinity();
  const int shifts[] = {-24, -12, 0, 12, 24};

  for (int shift : shifts) {
    std::vector<int> candidate;
    candidate.reserve(base_midis.size());
    bool inside = true;
    for (int value : base_midis) {
      int shifted = value + shift;
      if (has_range) {
        if (shifted < spec.range_min || shifted > spec.range_max) {
          inside = false;
          break;
        }
      }
      candidate.push_back(shifted);
    }
    if (!inside) {
      continue;
    }
    double mean = 0.0;
    for (int value : candidate) {
      mean += static_cast<double>(value);
    }
    mean /= static_cast<double>(candidate.size());
    double dist = std::abs(mean - target);
    if (dist < best_dist) {
      best_dist = dist;
      best_midis = candidate;
    }
  }

  if (has_range) {
    for (int& value : best_midis) {
      if (value < spec.range_min) {
        value = spec.range_min;
      } else if (value > spec.range_max) {
        value = spec.range_max;
      }
    }
  }

  return best_midis;
}

} // namespace

AbstractSample MelodySampler::next(const SessionSpec& spec, std::uint64_t& rng_state) {
  std::vector<int> degrees;
  std::vector<int> midis;

  for (int attempt = 0; attempt < kMaxTries; ++attempt) {
    degrees = generate_degrees(spec, rng_state);
    if (kRecentCapacity == 0) {
      break;
    }
    if (std::find(recent_sequences_.begin(), recent_sequences_.end(), degrees) == recent_sequences_.end()) {
      break;
    }
  }

  if (kRecentCapacity > 0) {
    recent_sequences_.push_back(degrees);
    if (recent_sequences_.size() > kRecentCapacity) {
      recent_sequences_.pop_front();
    }
  }

  midis = degrees_to_midi(spec, degrees);

  nlohmann::json data = nlohmann::json::object();
  nlohmann::json degree_array = nlohmann::json::array();
  for (int degree : degrees) {
    degree_array.push_back(degree);
  }
  nlohmann::json midi_array = nlohmann::json::array();
  for (int midi : midis) {
    midi_array.push_back(midi);
  }
  data["degrees"] = degree_array;
  data["midi"] = midi_array;

  return AbstractSample{"melody", data};
}

MelodyDrill::DrillOutput MelodyDrill::make_question(const SessionSpec& spec, const AbstractSample& sample) {
  const auto& degrees_json = sample.degrees.value("degrees", nlohmann::json::array());
  const auto& midi_json = sample.degrees.value("midi", nlohmann::json::array());

  PromptPlan plan;
  plan.modality = "midi";
  plan.tempo_bpm = spec.tempo_bpm;
  plan.count_in = true;

  nlohmann::json note_payload = nlohmann::json::array();
  for (const auto& entry : midi_json.get_array()) {
    int pitch = entry.get<int>();
    plan.notes.push_back({pitch, 400, std::nullopt, std::nullopt});
    note_payload.push_back(pitch);
  }

  nlohmann::json question_payload = nlohmann::json::object();
  question_payload["midi"] = note_payload;
  question_payload["degrees"] = degrees_json;

  nlohmann::json answer_payload = nlohmann::json::object();
  answer_payload["degrees"] = degrees_json;

  nlohmann::json hints = nlohmann::json::object();
  hints["answer_kind"] = "melody_notes";
  nlohmann::json allowed = nlohmann::json::array();
  allowed.push_back("Replay");
  allowed.push_back("TempoDown");
  hints["allowed_assists"] = allowed;
  nlohmann::json budget = nlohmann::json::object();
  for (const auto& entry : spec.assistance_policy) {
    budget[entry.first] = entry.second;
  }
  hints["assist_budget"] = budget;

  DrillModule::DrillOutput output{TypedPayload{"melody", question_payload},
                                  TypedPayload{"melody_notes", answer_payload},
                                  plan,
                                  hints};
  return output;
}

} // namespace ear

