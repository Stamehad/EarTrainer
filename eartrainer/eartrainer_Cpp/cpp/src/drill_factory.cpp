#include "../include/ear/drill_factory.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

#include "../drills/common.hpp"        // parse_key_from_string etc.
#include "../drills/chord.hpp"
#include "../drills/interval.hpp"
#include "../drills/melody.hpp"
#include "../drills/note.hpp"
#include "../include/ear/types.hpp"
#include "../include/ear/midi_clip.hpp"

using nlohmann::json;

void DrillSpec::apply_defaults() {
  key = "C";
  assistance_policy.clear();
  nlohmann::json merged_params = j_params.is_object() ? j_params : nlohmann::json::object();

  if (defaults.is_object()) {
    const auto& obj = defaults.get_object();
    if (auto it = obj.find("key"); it != obj.end() && it->second.is_string()) {
      key = it->second.get<std::string>();
    }
    if (auto it = obj.find("assistance_policy"); it != obj.end() && it->second.is_object()) {
      for (const auto& kv : it->second.get_object()) {
        assistance_policy[kv.first] = kv.second.get<int>();
      }
    }
  }

  if (merged_params.is_object()) {
    if (!merged_params.contains("range_below_semitones") && merged_params.contains("note_range_semitones")) {
      merged_params["range_below_semitones"] = merged_params["note_range_semitones"];
    }
    if (!merged_params.contains("range_above_semitones") && merged_params.contains("note_range_semitones")) {
      merged_params["range_above_semitones"] = merged_params["note_range_semitones"];
    }
  }
  j_params = merged_params;
}

DrillSpec DrillSpec::from_session(const ear::SessionSpec& spec) {
  DrillSpec out;
  out.id = spec.drill_kind;
  out.family = spec.drill_kind;
  out.level = spec.inspect_level;
  out.tier = spec.inspect_tier;
  out.key = spec.key;
  out.params = spec.params;
  out.assistance_policy = spec.assistance_policy;
  out.j_params = spec.sampler_params.is_object() ? spec.sampler_params : nlohmann::json::object();
  out.defaults["key"] = out.key;
  if (!out.assistance_policy.empty()) {
    nlohmann::json assists = nlohmann::json::object();
    for (const auto& kv : out.assistance_policy) {
      assists[kv.first] = kv.second;
    }
    out.defaults["assistance_policy"] = assists;
  }
  return out;
}

// ------------------------ DrillFactory impl ------------------------

namespace ear {

DrillFactory& DrillFactory::instance() {
  static DrillFactory f;
  return f;
}

void DrillFactory::register_family(const std::string& family, Creator create) {
  registry_[family] = std::move(create);
}

std::unique_ptr<DrillModule> DrillFactory::create_module(const std::string& family) const {
  auto it = registry_.find(family);
  if (it == registry_.end()) {
    throw std::runtime_error("DrillFactory: family not registered: " + family);
  }
  auto module = it->second();
  if (!module) {
    throw std::runtime_error("DrillFactory: creator returned null for family: " + family);
  }
  return module;
}

DrillAssignment DrillFactory::create(const DrillSpec& spec) const {
  DrillAssignment assignment;
  assignment.id = spec.id;
  assignment.family = spec.family;
  assignment.spec = spec;
  assignment.module = create_module(spec.family);
  assignment.module->configure(assignment.spec);
  return assignment;
}

std::vector<DrillAssignment> DrillFactory::create_for_level(const std::vector<DrillSpec>& all, int level) const {
  std::vector<DrillSpec> specs;
  for (const auto& spec : all) {
    if (spec.level == level) {
      specs.push_back(spec);
    }
  }
  std::vector<DrillAssignment> out;
  out.reserve(specs.size());
  for (const auto& s : specs) out.push_back(create(s));
  return out;
}

namespace {

int json_number_to_int(const json& node, int fallback) {
  if (node.is_number_integer()) {
    return node.get<int>();
  }
  if (node.is_number_float()) {
    return static_cast<int>(std::lround(node.get<double>()));
  }
  if (node.is_number()) {
    return static_cast<int>(std::lround(node.get<double>()));
  }
  return fallback;
}

} // namespace

// void apply_prompt_rendering(const DrillSpec& spec, QuestionsBundle& bundle) {
//   if (!bundle.prompt.has_value()) {
//     return;
//   }
//   auto& plan = bundle.prompt.value();
//   if (plan.modality == "midi-clip" && plan.midi_clip.has_value()) {
//     return;
//   }
//   if (!bundle.ui_hints.is_object() || !bundle.ui_hints.contains("prompt_manifest")) {
//     return;
//   }
//   const auto& manifest = bundle.ui_hints["prompt_manifest"];
//   if (!manifest.is_object()) {
//     return;
//   }

//   std::string format;
//   if (manifest.contains("format") && manifest["format"].is_string()) {
//     format = manifest["format"].get<std::string>();
//   }

//   if (format == "midi-clip" && manifest.contains("midi_clip")) {
//     plan.modality = "midi-clip";
//     plan.midi_clip = manifest["midi_clip"];
//     return;
//   }

//   if (format != "multi_track_prompt/v1") {
//     return;
//   }

//   int tempo_default = 90;
//   if (plan.tempo_bpm.has_value() && plan.tempo_bpm.value() > 0) {
//     tempo_default = plan.tempo_bpm.value();
//   }

//   int tempo = tempo_default;
//   if (manifest.contains("tempo_bpm")) {
//     tempo = json_number_to_int(manifest["tempo_bpm"], tempo_default);
//     if (tempo <= 0) {
//       tempo = tempo_default;
//     }
//   }

//   int ppq = 480;
//   if (manifest.contains("ppq")) {
//     ppq = std::max(1, json_number_to_int(manifest["ppq"], ppq));
//   }

//   MidiClipBuilder builder(tempo, ppq);

//   if (!manifest.contains("tracks") || !manifest["tracks"].is_array()) {
//     return;
//   }
//   const auto& tracks = manifest["tracks"].get_array();

//   int manifest_default_dur = 0;
//   if (manifest.contains("default_dur_ms")) {
//     manifest_default_dur = std::max(0, json_number_to_int(manifest["default_dur_ms"], 0));
//   }
//   int manifest_default_velocity = 90;
//   if (manifest.contains("default_velocity")) {
//     manifest_default_velocity = std::clamp(json_number_to_int(manifest["default_velocity"], 90), 0, 127);
//   }

//   for (const auto& track_node_json : tracks) {
//     const auto& track_node = track_node_json;
//     if (!track_node.is_object()) {
//       continue;
//     }
//     std::string track_name = "track";
//     if (track_node.contains("name") && track_node["name"].is_string()) {
//       track_name = track_node["name"].get<std::string>();
//     }
//     int channel = 0;
//     if (track_node.contains("channel")) {
//       channel = json_number_to_int(track_node["channel"], 0);
//     }
//     int program = 0;
//     if (track_node.contains("program")) {
//       program = json_number_to_int(track_node["program"], 0);
//     }

//     int track_velocity = manifest_default_velocity;
//     if (track_node.contains("velocity")) {
//       track_velocity = std::clamp(json_number_to_int(track_node["velocity"], track_velocity), 0, 127);
//     }
//     int track_default_dur = manifest_default_dur;
//     if (track_node.contains("dur_ms")) {
//       track_default_dur = std::max(0, json_number_to_int(track_node["dur_ms"], track_default_dur));
//     }

//     int track_index = builder.add_track(track_name, channel, program);

//     if (!track_node.contains("notes") || !track_node["notes"].is_array()) {
//       continue;
//     }
//     const auto& notes = track_node["notes"].get_array();
//     for (const auto& note_node_json : notes) {
//       const auto& note_node = note_node_json;
//       if (!note_node.is_object() || !note_node.contains("midi")) {
//         continue;
//       }
//       int midi = json_number_to_int(note_node["midi"], -1);
//       if (midi < 0) {
//         continue;
//       }
//       int start_ms = 0;
//       if (note_node.contains("start_ms")) {
//         start_ms = std::max(0, json_number_to_int(note_node["start_ms"], 0));
//       }
//       int dur_ms = track_default_dur;
//       if (note_node.contains("dur_ms")) {
//         dur_ms = std::max(0, json_number_to_int(note_node["dur_ms"], dur_ms));
//       }
//       if (dur_ms <= 0) {
//         dur_ms = manifest_default_dur;
//       }
//       if (dur_ms <= 0 && !plan.notes.empty()) {
//         dur_ms = plan.notes.front().dur_ms;
//       }
//       if (dur_ms <= 0) {
//         dur_ms = 500;
//       }

//       int velocity = track_velocity;
//       if (note_node.contains("velocity")) {
//         velocity = std::clamp(json_number_to_int(note_node["velocity"], velocity), 0, 127);
//       }

//       int start_ticks = builder.ms_to_ticks(start_ms);
//       int dur_ticks = builder.ms_to_ticks(dur_ms);
//       if (dur_ticks <= 0) {
//         dur_ticks = 1;
//       }

//       builder.add_note_ticks(track_index, start_ticks, dur_ticks, midi, std::optional<int>(velocity));
//     }
//   }

//   plan.modality = "midi-clip";
//   plan.midi_clip = to_json(builder.build());
// }

// ------------------------ Concrete bindings ------------------------

void register_builtin_drills(DrillFactory& factory) {
  factory.register_family("melody", []() { return std::make_unique<MelodyDrill>(); });

  factory.register_family("note", []() { return std::make_unique<NoteDrill>(); });

  factory.register_family("interval", []() { return std::make_unique<IntervalDrill>(); });

  factory.register_family("chord", []() { return std::make_unique<ChordDrill>(); });
  factory.register_family("chord_melody", []() { return std::make_unique<ChordDrill>(); });
}

} // namespace ear
