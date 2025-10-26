#include "melody.hpp"

#include "common.hpp"
#include "../src/rng.hpp"
#include "../include/ear/question_bundle_v2.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <optional>
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

std::vector<int> generate_degrees(const MelodyParams& params, std::uint64_t& rng_state) {
  int idx = rand_int(rng_state, 0, params.melody_lengths.size()-1);
  int length = params.melody_lengths[static_cast<std::size_t>(idx)];

  int max_step = params.melody_max_step;
  max_step = std::clamp(max_step, 0, 7);
  

  std::vector<int> all_degrees = base_degrees();
  int start_index = rand_int(rng_state, 0, static_cast<int>(all_degrees.size()) - 1);
  std::vector<int> sequence;
  sequence.reserve(static_cast<std::size_t>(length));
  sequence.push_back(all_degrees[static_cast<std::size_t>(start_index)]);

  MelodyState state;

  for (int i = 1; i < length; ++i) {
    auto weights = make_base_weights();
    if (max_step < 7) {
      for (std::size_t idx = 0; idx < weights.size(); ++idx) {
        if (std::abs(kSteps[idx]) > max_step) {
          weights[idx] = 0.0;
        }
      }
    }
    apply_musical_modifiers(weights, state);
    auto probs = normalise(weights);
    int step = choose_step(rng_state, probs);

    int last_degree = sequence.back();
    int next_degree = last_degree + step;
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

std::vector<int> degrees_to_midi(
  const DrillSpec& spec, 
  const std::vector<int>& degrees, 
  const std::pair<int,int> midi_range
) {
  int tonic = drills::central_tonic_midi(spec.key);
  
  const int midi_min = midi_range.first;
  const int midi_max = midi_range.second;
  std::vector<int> base_midis;
  base_midis.reserve(degrees.size());
  for (int degree : degrees) {
    base_midis.push_back(tonic + drills::degree_to_offset(degree));
  }

  double target = static_cast<double>(tonic);
  bool has_range = midi_min < midi_max;
  if (has_range) {
    target = static_cast<double>(midi_min + midi_max) / 2.0;
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
        if (shifted < midi_min || shifted > midi_max) {
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
      if (value < midi_min) {
        value = midi_min;
      } else if (value > midi_max) {
        value = midi_max;
      }
    }
  }

  return best_midis;
}

} // namespace

//====================================================================
// INITIALIZE MELODYDRILL
//====================================================================
void MelodyDrill::configure(const DrillSpec& spec) {
  spec_ = spec;
  params = std::get<MelodyParams>(spec_.params);
  tonic_midi = drills::central_tonic_midi(spec_.key);
  midi_range = {
    tonic_midi - params.range_below_semitones, 
    tonic_midi + params.range_above_semitones
  };

  recent_sequences_.clear();
}

//====================================================================
// NEXT QUESTION -> QUESTION BUNDLE
//====================================================================
QuestionBundle MelodyDrill::next_question(std::uint64_t& rng_state) {
  std::vector<int> degrees;
  std::vector<int> midis;

  for (int attempt = 0; attempt < kMaxTries; ++attempt) {
    degrees = generate_degrees(params, rng_state);
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

  midis = degrees_to_midi(spec_, degrees, midi_range);

  //-----------------------------------------------------
  // PREPARE QUESTION AND ANSWER
  //-----------------------------------------------------
  MelodyAnswerV2 melody_answer = MelodyAnswerV2{degrees};
  MelodyQuestionV2 melody_question = MelodyQuestionV2{
   tonic_midi, spec_.key, spec_.quality, degrees
  };

  //-----------------------------------------------------------------
  // GENERATE MIDI-CLIP
  //-----------------------------------------------------------------
  MidiClipBuilder b(params.tempo_bpm, 480);
  auto melody_track = b.add_track("melody", 0, params.program);

  Beats beat = Beats{0}; // CURRENT BEAT
  for (int midi : midis){
    b.add_note(melody_track, beat, Beats{params.note_beat}, midi, params.velocity);
    beat.advance_by(params.note_beat);
  }

  //-----------------------------------------------------------------
  // GENERATE QUESTION BUNDLE
  //-----------------------------------------------------------------
  ear::QuestionBundle bundle;
  bundle.question_id = "place-holder";
  bundle.question_id.clear();
  bundle.correct_answer = melody_answer;
  bundle.question = melody_question;
  bundle.prompt_clip = b.build();
  
  return bundle;
  }

} // namespace ear
