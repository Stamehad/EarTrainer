#include "json_bridge.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>

#include "../drills/common.hpp"
#include "../include/resources/drill_params.hpp"

namespace ear::bridge {
namespace {

template <typename Setter>
bool assign_if_present(const nlohmann::json& obj, const char* key, Setter&& setter) {
  if (!obj.contains(key)) {
    return false;
  }
  const auto& value = obj[key];
  if (value.is_null()) {
    return false;
  }
  setter(value);
  return true;
}

int json_to_int(const nlohmann::json& value, std::string_view key) {
  if (value.is_number_integer()) {
    return value.get<int>();
  }
  if (value.is_number_float()) {
    return static_cast<int>(std::lround(value.get<double>()));
  }
  throw std::invalid_argument("Expected integer for field '" + std::string(key) + "'");
}

double json_to_double(const nlohmann::json& value, std::string_view key) {
  if (!value.is_number()) {
    throw std::invalid_argument("Expected number for field '" + std::string(key) + "'");
  }
  return value.get<double>();
}

bool json_to_bool(const nlohmann::json& value, std::string_view key) {
  if (value.is_boolean()) {
    return value.get<bool>();
  }
  if (value.is_number_integer()) {
    const int v = value.get<int>();
    if (v == 0 || v == 1) {
      return v != 0;
    }
  }
  throw std::invalid_argument("Expected bool for field '" + std::string(key) + "'");
}

std::vector<int> json_to_int_vector(const nlohmann::json& value, std::string_view key) {
  if (!value.is_array()) {
    throw std::invalid_argument("Expected array<int> for field '" + std::string(key) + "'");
  }
  std::vector<int> out;
  const auto length = value.size();
  out.reserve(length);
  for (std::size_t i = 0; i < length; ++i) {
    out.push_back(json_to_int(value[i], key));
  }
  return out;
}

std::string json_to_string(const nlohmann::json& value, std::string_view key) {
  if (!value.is_string()) {
    throw std::invalid_argument("Expected string for field '" + std::string(key) + "'");
  }
  return value.get<std::string>();
}

DrillInstrument json_to_instrument(const nlohmann::json& value, std::string_view key) {
  const int inst = json_to_int(value, key);
  switch (inst) {
    case 0: return DrillInstrument::Piano;
    case 1: return DrillInstrument::Strings;
    default:
      throw std::invalid_argument("Unknown instrument value for field '" + std::string(key) + "'");
  }
}

ChordDelivery json_to_delivery(const nlohmann::json& value, std::string_view key) {
  const int delivery = json_to_int(value, key);
  switch (delivery) {
    case 0: return ChordDelivery::Together;
    case 1: return ChordDelivery::Arpeggio;
    default:
      throw std::invalid_argument("Unknown delivery value for field '" + std::string(key) + "'");
  }
}

std::optional<NoteParams::TonicAnchor> json_to_tonic_anchor(const nlohmann::json& value,
                                                            std::string_view key) {
  if (value.is_null()) {
    return std::nullopt;
  }
  const int v = json_to_int(value, key);
  if (v < 0) {
    return std::nullopt;
  }
  switch (v) {
    case 0: return NoteParams::TonicAnchor::Before;
    case 1: return NoteParams::TonicAnchor::After;
    default:
      throw std::invalid_argument("Unknown tonic_anchor value for field '" + std::string(key) + "'");
  }
}

std::optional<bool> json_to_optional_bool(const nlohmann::json& value, std::string_view key) {
  if (value.is_null()) {
    return std::nullopt;
  }
  return json_to_bool(value, key);
}

std::optional<bool> json_to_tristate_bool(const nlohmann::json& value, std::string_view key) {
  if (value.is_null()) {
    return std::nullopt;
  }
  const int v = json_to_int(value, key);
  if (v < 0) {
    return std::nullopt;
  }
  if (v == 0) {
    return false;
  }
  if (v == 1) {
    return true;
  }
  throw std::invalid_argument("Unknown tri-state value for field '" + std::string(key) + "'");
}

nlohmann::json ints_to_json_array(const std::vector<int>& values) {
  nlohmann::json arr = nlohmann::json::array();
  for (int v : values) {
    arr.push_back(v);
  }
  return arr;
}

NoteParams parse_note_params(const nlohmann::json& overrides) {
  NoteParams params;
  assign_if_present(overrides, "allowed_degrees", [&](const nlohmann::json& value) {
    params.degrees = json_to_int_vector(value, "allowed_degrees");
  });
  assign_if_present(overrides, "avoid_repeat", [&](const nlohmann::json& value) {
    params.avoid_repeat = json_to_bool(value, "avoid_repeat");
  });
  assign_if_present(overrides, "range_below_semitones", [&](const nlohmann::json& value) {
    params.range_down = json_to_int(value, "range_below_semitones");
  });
  assign_if_present(overrides, "range_above_semitones", [&](const nlohmann::json& value) {
    params.range_up = json_to_int(value, "range_above_semitones");
  });
  assign_if_present(overrides, "inst", [&](const nlohmann::json& value) {
    params.inst = json_to_instrument(value, "inst");
  });
  assign_if_present(overrides, "tempo_bpm", [&](const nlohmann::json& value) {
    params.bpm = json_to_int(value, "tempo_bpm");
  });
  assign_if_present(overrides, "note_beats", [&](const nlohmann::json& value) {
    params.note_beats = json_to_double(value, "note_beats");
  });
  assign_if_present(overrides, "program", [&](const nlohmann::json& value) {
    params.program = json_to_int(value, "program");
  });
  assign_if_present(overrides, "velocity", [&](const nlohmann::json& value) {
    params.velocity = json_to_int(value, "velocity");
  });
  assign_if_present(overrides, "use_pathway", [&](const nlohmann::json& value) {
    params.pathway = json_to_bool(value, "use_pathway");
  });
  assign_if_present(overrides, "pathway_repeat_lead", [&](const nlohmann::json& value) {
    params.pathway_repeat_lead = json_to_bool(value, "pathway_repeat_lead");
  });
  assign_if_present(overrides, "pathway_beats", [&](const nlohmann::json& value) {
    params.pathway_beats = json_to_double(value, "pathway_beats");
  });
  assign_if_present(overrides, "pathway_rest", [&](const nlohmann::json& value) {
    params.pathway_rest = json_to_double(value, "pathway_rest");
  });
  assign_if_present(overrides, "note_step_beats", [&](const nlohmann::json& value) {
    params.note_step_beats = json_to_double(value, "note_step_beats");
  });
  assign_if_present(overrides, "note_tempo_bpm", [&](const nlohmann::json& value) {
    params.note_tempo_bpm = json_to_int(value, "note_tempo_bpm");
  });
  assign_if_present(overrides, "use_anchor", [&](const nlohmann::json& value) {
    params.use_anchor = json_to_bool(value, "use_anchor");
  });
  assign_if_present(overrides, "tonic_anchor", [&](const nlohmann::json& value) {
    params.tonic_anchor = json_to_tonic_anchor(value, "tonic_anchor");
  });
  assign_if_present(overrides, "tonic_anchor_include_octave", [&](const nlohmann::json& value) {
    params.tonic_anchor_include_octave = json_to_bool(value, "tonic_anchor_include_octave");
  });
  return params;
}

IntervalParams parse_interval_params(const nlohmann::json& overrides) {
  IntervalParams params;
  assign_if_present(overrides, "tempo_bpm", [&](const nlohmann::json& value) {
    params.bpm = json_to_int(value, "tempo_bpm");
  });
  assign_if_present(overrides, "note_beat", [&](const nlohmann::json& value) {
    params.note_beat = json_to_double(value, "note_beat");
  });
  assign_if_present(overrides, "program", [&](const nlohmann::json& value) {
    params.program = json_to_int(value, "program");
  });
  assign_if_present(overrides, "allowed_bottom_degrees", [&](const nlohmann::json& value) {
    params.allowed_bottom_degrees = json_to_int_vector(value, "allowed_bottom_degrees");
  });
  assign_if_present(overrides, "allowed_degrees", [&](const nlohmann::json& value) {
    params.allowed_degrees = json_to_int_vector(value, "allowed_degrees");
  });
  assign_if_present(overrides, "allowed_sizes", [&](const nlohmann::json& value) {
    params.intervals = json_to_int_vector(value, "allowed_sizes");
  });
  assign_if_present(overrides, "avoid_repeat", [&](const nlohmann::json& value) {
    params.avoid_repeat = json_to_bool(value, "avoid_repeat");
  });
  assign_if_present(overrides, "range_semitones", [&](const nlohmann::json& value) {
    params.range_semitones = json_to_int(value, "range_semitones");
  });
  assign_if_present(overrides, "velocity", [&](const nlohmann::json& value) {
    params.velocity = json_to_int(value, "velocity");
  });
  assign_if_present(overrides, "inst", [&](const nlohmann::json& value) {
    params.inst = json_to_instrument(value, "inst");
  });
  assign_if_present(overrides, "cluster_ids", [&](const nlohmann::json& value) {
    params.cluster_ids = json_to_int_vector(value, "cluster_ids");
  });
  assign_if_present(overrides, "add_helper", [&](const nlohmann::json& value) {
    params.helper = json_to_bool(value, "add_helper");
  });
  return params;
}

MelodyParams parse_melody_params(const nlohmann::json& overrides) {
  MelodyParams params;
  assign_if_present(overrides, "tempo_bpm", [&](const nlohmann::json& value) {
    params.bpm = json_to_int(value, "tempo_bpm");
  });
  assign_if_present(overrides, "program", [&](const nlohmann::json& value) {
    params.program = json_to_int(value, "program");
  });
  assign_if_present(overrides, "melody_lengths", [&](const nlohmann::json& value) {
    params.length = json_to_int_vector(value, "melody_lengths");
  });
  assign_if_present(overrides, "melody_max_step", [&](const nlohmann::json& value) {
    params.max_step = json_to_int(value, "melody_max_step");
  });
  assign_if_present(overrides, "avoid_repeat", [&](const nlohmann::json& value) {
    params.avoid_repeat = json_to_bool(value, "avoid_repeat");
  });
  assign_if_present(overrides, "range_below_semitones", [&](const nlohmann::json& value) {
    params.range_down = json_to_int(value, "range_below_semitones");
  });
  assign_if_present(overrides, "range_above_semitones", [&](const nlohmann::json& value) {
    params.range_up = json_to_int(value, "range_above_semitones");
  });
  assign_if_present(overrides, "note_beat", [&](const nlohmann::json& value) {
    params.note_beat = json_to_double(value, "note_beat");
  });
  assign_if_present(overrides, "velocity", [&](const nlohmann::json& value) {
    params.velocity = json_to_int(value, "velocity");
  });
  assign_if_present(overrides, "inst", [&](const nlohmann::json& value) {
    params.inst = json_to_instrument(value, "inst");
  });
  return params;
}

ChordParams parse_chord_params(const nlohmann::json& overrides) {
  ChordParams params;
  assign_if_present(overrides, "allowed_degrees", [&](const nlohmann::json& value) {
    params.degrees = json_to_int_vector(value, "allowed_degrees");
  });
  assign_if_present(overrides, "inst", [&](const nlohmann::json& value) {
    params.inst = json_to_instrument(value, "inst");
  });
  assign_if_present(overrides, "delivery", [&](const nlohmann::json& value) {
    params.delivery = json_to_delivery(value, "delivery");
  });
  assign_if_present(overrides, "allowed_top_degrees", [&](const nlohmann::json& value) {
    params.allowed_top_degrees = json_to_int_vector(value, "allowed_top_degrees");
  });
  assign_if_present(overrides, "sequence_lengths", [&](const nlohmann::json& value) {
    params.sequence_lengths = json_to_int_vector(value, "sequence_lengths");
  });
  assign_if_present(overrides, "avoid_repeat", [&](const nlohmann::json& value) {
    params.avoid_repeat = json_to_bool(value, "avoid_repeat");
  });
  assign_if_present(overrides, "chord_avoid_repeat", [&](const nlohmann::json& value) {
    params.chord_avoid_repeat = json_to_tristate_bool(value, "chord_avoid_repeat");
  });
  assign_if_present(overrides, "range_semitones", [&](const nlohmann::json& value) {
    params.range_semitones = json_to_int(value, "range_semitones");
  });
  assign_if_present(overrides, "add_seventh", [&](const nlohmann::json& value) {
    params.add_seventh = json_to_bool(value, "add_seventh");
  });
  assign_if_present(overrides, "tempo_bpm", [&](const nlohmann::json& value) {
    params.bpm = json_to_int(value, "tempo_bpm");
  });
  assign_if_present(overrides, "right_voicing_id", [&](const nlohmann::json& value) {
    params.right_voicing_id = json_to_string(value, "right_voicing_id");
  });
  assign_if_present(overrides, "bass_voicing_id", [&](const nlohmann::json& value) {
    params.bass_voicing_id = json_to_string(value, "bass_voicing_id");
  });
  assign_if_present(overrides, "voicing_profile", [&](const nlohmann::json& value) {
    const std::string profile = json_to_string(value, "voicing_profile");
    if (profile.empty()) {
      params.voicing_profile.reset();
    } else {
      params.voicing_profile = profile;
    }
  });
  // // assign_if_present(overrides, "prompt_split_tracks", [&](const nlohmann::json& value) {
  // //   params.prompt_split_tracks = json_to_bool(value, "prompt_split_tracks");
  // // });
  // // assign_if_present(overrides, "prompt_program", [&](const nlohmann::json& value) {
  // //   params.prompt_program = json_to_int(value, "prompt_program");
  // // });
  // // assign_if_present(overrides, "prompt_channel", [&](const nlohmann::json& value) {
  // //   params.prompt_channel = json_to_int(value, "prompt_channel");
  // // });
  // assign_if_present(overrides, "right_program", [&](const nlohmann::json& value) {
  //   params.right_program = json_to_int(value, "right_program");
  // });
  assign_if_present(overrides, "right_channel", [&](const nlohmann::json& value) {
    params.right_channel = json_to_int(value, "right_channel");
  });
  // assign_if_present(overrides, "bass_program", [&](const nlohmann::json& value) {
  //   params.bass_program = json_to_int(value, "bass_program");
  // });
  assign_if_present(overrides, "bass_channel", [&](const nlohmann::json& value) {
    params.bass_channel = json_to_int(value, "bass_channel");
  });
  assign_if_present(overrides, "velocity", [&](const nlohmann::json& value) {
    params.velocity = json_to_int(value, "velocity");
  });
  assign_if_present(overrides, "dur_beats", [&](const nlohmann::json& value) {
    params.dur_beats = json_to_double(value, "dur_beats");
  });
  // assign_if_present(overrides, "duration_ms", [&](const nlohmann::json& value) {
  //   params.duration_ms = json_to_int(value, "duration_ms");
  // });
  assign_if_present(overrides, "strum_step_ms", [&](const nlohmann::json& value) {
    params.strum_step_ms = json_to_int(value, "strum_step_ms");
  });
  assign_if_present(overrides, "voice_leading_continuity", [&](const nlohmann::json& value) {
    params.voice_leading_continuity = json_to_bool(value, "voice_leading_continuity");
  });

  const nlohmann::json* training_root = nullptr;
  if (overrides.contains("training_root") && overrides["training_root"].is_object()) {
    training_root = &overrides["training_root"];
  }

  auto assign_training = [&](const char* dotted, const char* nested_key, auto&& setter) {
    if (assign_if_present(overrides, dotted, setter)) {
      return;
    }
    if (training_root) {
      assign_if_present(*training_root, nested_key, setter);
    }
  };

  assign_training("training_root.enabled", "enabled", [&](const nlohmann::json& value) {
    params.play_root.enabled = json_to_bool(value, "training_root.enabled");
  });
  assign_training("training_root.delay_beats", "delay_beats", [&](const nlohmann::json& value) {
    params.play_root.delay_beats = json_to_double(value, "training_root.delay_beats");
  });
  assign_training("training_root.dur_beats", "dur_beats", [&](const nlohmann::json& value) {
    params.play_root.dur_beats = json_to_double(value, "training_root.dur_beats");
  });
  assign_training("training_root.channel", "channel", [&](const nlohmann::json& value) {
    params.play_root.channel = json_to_int(value, "training_root.channel");
  });
  assign_training("training_root.program", "program", [&](const nlohmann::json& value) {
    params.play_root.program = json_to_int(value, "training_root.program");
  });
  assign_training("training_root.velocity", "velocity", [&](const nlohmann::json& value) {
    params.play_root.velocity = json_to_int(value, "training_root.velocity");
  });
  // assign_training("training_root.duration_ms", "duration_ms", [&](const nlohmann::json& value) {
  //   params.play_root.duration_ms = json_to_int(value, "training_root.duration_ms");
  // });
  return params;
}

DrillParams params_from_json(const std::string& kind, const nlohmann::json& overrides) {
  if (!overrides.is_object()) {
    throw std::invalid_argument("params must be an object");
  }
  if (kind == "note") {
    return DrillParams{parse_note_params(overrides)};
  }
  if (kind == "interval") {
    return DrillParams{parse_interval_params(overrides)};
  }
  if (kind == "melody") {
    return DrillParams{parse_melody_params(overrides)};
  }
  if (kind == "chord" || kind == "chord_melody") {
    return DrillParams{parse_chord_params(overrides)};
  }
  // Unknown drill kinds fall back to defaults.
  return DrillParams{};
}

int instrument_to_int(DrillInstrument inst) {
  switch (inst) {
    case DrillInstrument::Strings: return 1;
    case DrillInstrument::Piano:
    default: return 0;
  }
}

int delivery_to_int(ChordDelivery delivery) {
  switch (delivery) {
    case ChordDelivery::Arpeggio: return 1;
    case ChordDelivery::Together:
    default: return 0;
  }
}

int tonic_anchor_to_int(const std::optional<NoteParams::TonicAnchor>& anchor) {
  if (!anchor.has_value()) {
    return -1;
  }
  return (*anchor == NoteParams::TonicAnchor::Before) ? 0 : 1;
}

int optional_bool_to_int(const std::optional<bool>& value) {
  if (!value.has_value()) {
    return -1;
  }
  return value.value() ? 1 : 0;
}

nlohmann::json note_params_to_json(const NoteParams& params) {
  nlohmann::json json = nlohmann::json::object();
  json["allowed_degrees"] = ints_to_json_array(params.degrees);
  json["avoid_repeat"] = params.avoid_repeat;
  json["range_below_semitones"] = params.range_down;
  json["range_above_semitones"] = params.range_up;
  json["inst"] = instrument_to_int(params.inst);
  json["tempo_bpm"] = params.bpm;
  json["note_beats"] = params.note_beats;
  json["program"] = params.program;
  json["velocity"] = params.velocity;
  json["use_pathway"] = params.pathway;
  json["pathway_repeat_lead"] = params.pathway_repeat_lead;
  json["pathway_beats"] = params.pathway_beats;
  json["pathway_rest"] = params.pathway_rest;
  json["note_step_beats"] = params.note_step_beats;
  json["note_tempo_bpm"] = params.note_tempo_bpm;
  json["use_anchor"] = params.use_anchor;
  json["tonic_anchor"] = tonic_anchor_to_int(params.tonic_anchor);
  json["tonic_anchor_include_octave"] = params.tonic_anchor_include_octave;
  return json;
}

nlohmann::json interval_params_to_json(const IntervalParams& params) {
  nlohmann::json json = nlohmann::json::object();
  json["tempo_bpm"] = params.bpm;
  json["note_beat"] = params.note_beat;
  json["program"] = params.program;
  json["allowed_bottom_degrees"] = ints_to_json_array(params.allowed_bottom_degrees);
  json["allowed_degrees"] = ints_to_json_array(params.allowed_degrees);
  json["allowed_sizes"] = ints_to_json_array(params.intervals);
  json["avoid_repeat"] = params.avoid_repeat;
  json["range_semitones"] = params.range_semitones;
  json["velocity"] = params.velocity;
  json["inst"] = instrument_to_int(params.inst);
  json["cluster_ids"] = ints_to_json_array(params.cluster_ids);
  json["add_helper"] = params.helper;
  return json;
}

nlohmann::json melody_params_to_json(const MelodyParams& params) {
  nlohmann::json json = nlohmann::json::object();
  json["tempo_bpm"] = params.bpm;
  json["program"] = params.program;
  json["melody_lengths"] = ints_to_json_array(params.length);
  json["melody_max_step"] = params.max_step;
  json["avoid_repeat"] = params.avoid_repeat;
  json["range_below_semitones"] = params.range_down;
  json["range_above_semitones"] = params.range_up;
  json["note_beat"] = params.note_beat;
  json["velocity"] = params.velocity;
  json["inst"] = instrument_to_int(params.inst);
  return json;
}

nlohmann::json chord_params_to_json(const ChordParams& params) {
  nlohmann::json json = nlohmann::json::object();
  json["allowed_degrees"] = ints_to_json_array(params.degrees);
  json["inst"] = instrument_to_int(params.inst);
  json["delivery"] = delivery_to_int(params.delivery);
  json["allowed_top_degrees"] = ints_to_json_array(params.allowed_top_degrees);
  json["sequence_lengths"] = ints_to_json_array(params.sequence_lengths);
  json["avoid_repeat"] = params.avoid_repeat;
  json["chord_avoid_repeat"] = optional_bool_to_int(params.chord_avoid_repeat);
  json["range_semitones"] = params.range_semitones;
  json["add_seventh"] = params.add_seventh;
  json["tempo_bpm"] = params.bpm;
  json["right_voicing_id"] = params.right_voicing_id;
  json["bass_voicing_id"] = params.bass_voicing_id;
  json["voicing_profile"] = params.voicing_profile.has_value() ? params.voicing_profile.value() : std::string();
  // json["prompt_split_tracks"] = params.prompt_split_tracks;
  // json["prompt_program"] = params.prompt_program;
  // json["prompt_channel"] = params.prompt_channel;
  // json["right_program"] = params.right_program;
  // json["right_channel"] = params.right_channel;
  // json["bass_program"] = params.bass_program;
  json["bass_channel"] = params.bass_channel;
  json["velocity"] = params.velocity;
  json["dur_beats"] = params.dur_beats;
  // json["duration_ms"] = params.duration_ms;
  json["strum_step_ms"] = params.strum_step_ms;
  json["voice_leading_continuity"] = params.voice_leading_continuity;
  json["training_root.enabled"] = params.play_root.enabled;
  json["training_root.delay_beats"] = params.play_root.delay_beats;
  json["training_root.dur_beats"] = params.play_root.dur_beats;
  json["training_root.channel"] = params.play_root.channel;
  json["training_root.program"] = params.play_root.program;
  json["training_root.velocity"] = params.play_root.velocity;
  //json["training_root.duration_ms"] = params.play_root.duration_ms;
  return json;
}

nlohmann::json params_to_json(const DrillParams& params) {
  if (std::holds_alternative<std::monostate>(params)) {
    return nlohmann::json::object();
  }
  if (const auto* note = std::get_if<NoteParams>(&params)) {
    return note_params_to_json(*note);
  }
  if (const auto* interval = std::get_if<IntervalParams>(&params)) {
    return interval_params_to_json(*interval);
  }
  if (const auto* melody = std::get_if<MelodyParams>(&params)) {
    return melody_params_to_json(*melody);
  }
  if (const auto* chord = std::get_if<ChordParams>(&params)) {
    return chord_params_to_json(*chord);
  }
  return nlohmann::json::object();
}

// Convert the legacy PromptPlan (sequential notes with dur_ms) into a
// "midi-clip" JSON object ready for direct playback.
nlohmann::json prompt_plan_to_json_impl(const PromptPlan& plan) {
  if (plan.modality == "midi-clip" && plan.midi_clip.has_value()) {
    nlohmann::json json_plan = nlohmann::json::object();
    json_plan["modality"] = "midi-clip";
    json_plan["midi_clip"] = plan.midi_clip.value();
    return json_plan;
  }

  const int tempo = plan.tempo_bpm.has_value() ? plan.tempo_bpm.value() : 90;
  const int ppq = 480;
  const double ticks_per_ms = (tempo * ppq) / 60000.0; // ticks = dur_ms * ticks_per_ms

  nlohmann::json clip = nlohmann::json::object();
  clip["format"] = "midi-clip/v1";
  clip["ppq"] = ppq;
  clip["tempo_bpm"] = tempo;

  nlohmann::json track = nlohmann::json::object();
  track["name"] = "prompt";
  track["channel"] = 0;
  track["program"] = 0; // GM Acoustic Grand
  nlohmann::json events = nlohmann::json::array();

  int t = 0; // timeline in ticks
  std::vector<int> held;

  if (plan.modality == "midi_block") {
    // Simultaneous note_on at t=0; individual offs based on dur_ms
    for (const auto& n : plan.notes) {
      int dur_ticks = static_cast<int>(std::lround(n.dur_ms * ticks_per_ms));
      if (n.pitch < 0 || dur_ticks <= 0) {
        continue;
      }
      int vel = n.vel.has_value() ? n.vel.value() : 90;
      nlohmann::json on = nlohmann::json::object();
      on["t"] = 0;
      on["type"] = "note_on";
      on["note"] = n.pitch;
      on["vel"] = std::max(0, std::min(127, vel));
      events.push_back(on);

      nlohmann::json off = nlohmann::json::object();
      off["t"] = std::max(1, dur_ticks);
      off["type"] = "note_off";
      off["note"] = n.pitch;
      events.push_back(off);
    }
    t = 0;
    // length ticks = max off time
    int max_off = 0;
    for (std::size_t i = 0; i < events.size(); ++i) {
      const auto& ev = events[i];
      if (ev["type"].get<std::string>() == "note_off") {
        max_off = std::max(max_off, ev["t"].get<int>());
      }
    }
    t = max_off;
  } else {
    for (const auto& n : plan.notes) {
      int dur_ticks = static_cast<int>(std::lround(n.dur_ms * ticks_per_ms));
      if (n.pitch < 0 || dur_ticks <= 0) {
        t += std::max(0, dur_ticks);
        continue;
      }
      int vel = n.vel.has_value() ? n.vel.value() : 90;
      nlohmann::json on = nlohmann::json::object();
      on["t"] = t;
      on["type"] = "note_on";
      on["note"] = n.pitch;
      on["vel"] = std::max(0, std::min(127, vel));
      events.push_back(on);

      bool tie_forward = n.tie.has_value() && n.tie.value();
      if (!tie_forward) {
        nlohmann::json off = nlohmann::json::object();
        off["t"] = t + std::max(1, dur_ticks);
        off["type"] = "note_off";
        off["note"] = n.pitch;
        events.push_back(off);
      } else {
        held.push_back(n.pitch);
      }
      t += std::max(1, dur_ticks);
    }
    // Ensure all held notes are released at the end
    for (int pitch : held) {
      nlohmann::json off = nlohmann::json::object();
      off["t"] = t;
      off["type"] = "note_off";
      off["note"] = pitch;
      events.push_back(off);
    }
  }

  track["events"] = events;
  clip["length_ticks"] = t;
  nlohmann::json tracks = nlohmann::json::array();
  tracks.push_back(track);
  clip["tracks"] = tracks;

  nlohmann::json json_plan = nlohmann::json::object();
  json_plan["modality"] = "midi-clip";
  json_plan["midi_clip"] = clip;
  return json_plan;
}

PromptPlan prompt_from_json_impl(const nlohmann::json& json_plan) {
  // Legacy loader retained only for round-trips if needed. Not used by Python UI.
  PromptPlan plan;
  if (json_plan.contains("modality")) {
    plan.modality = json_plan["modality"].get<std::string>();
  } else {
    plan.modality = "midi-clip";
  }
  if (plan.modality == "midi-clip" && json_plan.contains("midi_clip") &&
      !json_plan["midi_clip"].is_null()) {
    plan.midi_clip = json_plan["midi_clip"];
  }
  // We no longer reconstruct sequential notes from midi-clip here.
  return plan;
}

nlohmann::json typed_to_json(const TypedPayload& payload) {
  nlohmann::json json_payload = nlohmann::json::object();
  json_payload["type"] = payload.type;
  json_payload["payload"] = payload.payload;
  return json_payload;
}

TypedPayload typed_from_json(const nlohmann::json& json_payload) {
  TypedPayload payload;
  payload.type = json_payload["type"].get<std::string>();
  payload.payload = json_payload["payload"];
  return payload;
}

// ----------------------------------------------------------------------------
// V2 Answer payload (variant) JSON helpers
// ----------------------------------------------------------------------------
static nlohmann::json answer_payload_v2_to_json(const AnswerPayloadV2& payload) {
  return std::visit(
      [](const auto& a) -> nlohmann::json {
        using T = std::decay_t<decltype(a)>;
        nlohmann::json j = nlohmann::json::object();
        if constexpr (std::is_same_v<T, ChordAnswerV2>) {
          j["type"] = "chord";
          auto to_int_array = [](const std::vector<int>& values) {
            nlohmann::json arr = nlohmann::json::array();
            for (int v : values) { arr.push_back(v); }
            return arr;
          };
          auto to_optional_array = [](const std::vector<std::optional<int>>& values) {
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& v : values) {
              arr.push_back(v.has_value() ? nlohmann::json(v.value()) : nlohmann::json(nullptr));
            }
            return arr;
          };
          auto to_bool_array = [](const std::vector<bool>& values) {
            nlohmann::json arr = nlohmann::json::array();
            for (bool v : values) { arr.push_back(v); }
            return arr;
          };
          j["root_degrees"] = to_int_array(a.root_degrees);
          j["bass_deg"] = to_optional_array(a.bass_deg);
          j["top_deg"] = to_optional_array(a.top_deg);
          j["expect_root"] = to_bool_array(a.expect_root);
          j["expect_bass"] = to_bool_array(a.expect_bass);
          j["expect_top"] = to_bool_array(a.expect_top);
        } else if constexpr (std::is_same_v<T, MelodyAnswerV2>) {
          j["type"] = "melody";
          nlohmann::json arr = nlohmann::json::array();
          for (int v : a.melody) { arr.push_back(v); }
          j["melody"] = std::move(arr);
        } else if constexpr (std::is_same_v<T, HarmonyAnswerV2>) {
          j["type"] = "harmony";
          nlohmann::json arr = nlohmann::json::array();
          for (int v : a.notes) { arr.push_back(v); }
          j["notes"] = std::move(arr);
        }
        return j;
      },
      payload);
}

static std::string triad_quality_to_string(TriadQuality quality) {
  switch (quality) {
    case TriadQuality::Major:
      return "major";
    case TriadQuality::Minor:
      return "minor";
    case TriadQuality::Diminished:
      return "diminished";
  }
  return "major";
}

static TriadQuality triad_quality_from_string(const std::string& quality) {
  if (quality == "minor") {
    return TriadQuality::Minor;
  }
  if (quality == "diminished" || quality == "dim") {
    return TriadQuality::Diminished;
  }
  return TriadQuality::Major;
}

static KeyQuality key_quality_from_string(const std::string& quality) {
  if (quality == "minor") {
    return KeyQuality::Minor;
  }
  return KeyQuality::Major;
}

static nlohmann::json question_payload_v2_to_json(const QuestionPayloadV2& payload) {
  return std::visit(
      [](const auto& q) -> nlohmann::json {
        using T = std::decay_t<decltype(q)>;
        nlohmann::json j = nlohmann::json::object();
        if constexpr (std::is_same_v<T, ChordQuestionV2>) {
          j["type"] = "chord";
          j["tonic_midi"] = q.tonic_midi;
          j["tonic"] = q.tonic;
          j["key"] = key_quality_to_string(q.key);
          auto to_int_array = [](const std::vector<int>& values) {
            nlohmann::json arr = nlohmann::json::array();
            for (int v : values) { arr.push_back(v); }
            return arr;
          };
          auto to_quality_array = [](const std::vector<TriadQuality>& values) {
            nlohmann::json arr = nlohmann::json::array();
            for (auto qv : values) { arr.push_back(triad_quality_to_string(qv)); }
            return arr;
          };
          auto to_optional_vec_array = [](const std::vector<std::optional<std::vector<int>>>& values) {
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& opt : values) {
              if (!opt.has_value()) {
                arr.push_back(nullptr);
              } else {
                nlohmann::json inner = nlohmann::json::array();
                for (int v : opt.value()) { inner.push_back(v); }
                arr.push_back(std::move(inner));
              }
            }
            return arr;
          };
          auto to_optional_array = [](const auto& values) {
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& opt : values) {
              if (opt.has_value()) {
                arr.push_back(opt.value());
              } else {
                arr.push_back(nullptr);
              }
            }
            return arr;
          };
          auto to_bool_array = [](const std::vector<bool>& values) {
            nlohmann::json arr = nlohmann::json::array();
            for (bool v : values) { arr.push_back(v); }
            return arr;
          };

          j["root_degrees"] = to_int_array(q.root_degrees);
          j["qualities"] = to_quality_array(q.qualities);
          j["rh_degrees"] = to_optional_vec_array(q.rh_degrees);
          j["bass_degrees"] = to_optional_array(q.bass_degrees);
          j["right_voicing_id"] = to_optional_array(q.right_voicing_ids);
          j["bass_voicing_id"] = to_optional_array(q.bass_voicing_ids);
          j["is_anchor"] = to_bool_array(q.is_anchor);

          // legacy single-chord mirror for older clients
          if (!q.root_degrees.empty()) {
            j["root_degree"] = q.root_degrees.front();
          }
          if (!q.qualities.empty()) {
            j["quality"] = triad_quality_to_string(q.qualities.front());
          }
        } else if constexpr (std::is_same_v<T, MelodyQuestionV2>) {
          j["type"] = "melody";
          j["tonic_midi"] = q.tonic_midi;
          j["tonic"] = q.tonic;
          j["key"] = key_quality_to_string(q.key);
          nlohmann::json melody = nlohmann::json::array();
          for (int value : q.melody) {
            melody.push_back(value);
          }
          j["melody"] = std::move(melody);
          if (q.octave.has_value()) {
            nlohmann::json arr = nlohmann::json::array();
            for (int value : q.octave.value()) {
              arr.push_back(value);
            }
            j["octave"] = std::move(arr);
          } else {
            j["octave"] = nullptr;
          }
          j["helper"] = q.helper.has_value() ? nlohmann::json(q.helper.value()) : nlohmann::json(nullptr);
        } else if constexpr (std::is_same_v<T, HarmonyQuestionV2>) {
          j["type"] = "harmony";
          j["tonic_midi"] = q.tonic_midi;
          j["tonic"] = q.tonic;
          j["key"] = key_quality_to_string(q.key);
          j["note_num"] = q.note_num;
          nlohmann::json notes = nlohmann::json::array();
          for (int value : q.notes) {
            notes.push_back(value);
          }
          j["notes"] = std::move(notes);
          j["interval"] = q.interval.has_value() ? nlohmann::json(q.interval.value()) : nlohmann::json(nullptr);
        }
        return j;
      },
      payload);
}

static QuestionPayloadV2 question_payload_v2_from_json(const nlohmann::json& json) {
  const std::string type = json.contains("type") && json["type"].is_string() ? json["type"].get<std::string>() : std::string();
  if (type == "chord") {
    ChordQuestionV2 q{};
    q.tonic_midi = json.contains("tonic_midi") ? json["tonic_midi"].get<int>() : 0;
    q.tonic = json.contains("tonic") && json["tonic"].is_string() ? json["tonic"].get<std::string>() : std::string();
    q.key = json.contains("key") && json["key"].is_string() ? key_quality_from_string(json["key"].get<std::string>())
                                                              : KeyQuality::Major;
    auto parse_int_array = [&](const char* key) {
      std::vector<int> values;
      if (json.contains(key) && json[key].is_array()) {
        const auto& arr = json[key];
        values.reserve(arr.size());
        for (std::size_t i = 0; i < arr.size(); ++i) {
          values.push_back(arr[i].get<int>());
        }
      }
      return values;
    };
    auto parse_quality_array = [&]() {
      std::vector<TriadQuality> values;
      if (json.contains("qualities") && json["qualities"].is_array()) {
        const auto& arr = json["qualities"];
        values.reserve(arr.size());
        for (std::size_t i = 0; i < arr.size(); ++i) {
          values.push_back(triad_quality_from_string(arr[i].get<std::string>()));
        }
      }
      return values;
    };
    auto parse_optional_vec_array = [&](const char* key) {
      std::vector<std::optional<std::vector<int>>> values;
      if (json.contains(key) && json[key].is_array()) {
        const auto& arr = json[key];
        values.reserve(arr.size());
        for (std::size_t i = 0; i < arr.size(); ++i) {
          if (arr[i].is_null()) {
            values.push_back(std::nullopt);
          } else if (arr[i].is_array()) {
            std::vector<int> inner;
            inner.reserve(arr[i].size());
            for (std::size_t j = 0; j < arr[i].size(); ++j) {
              inner.push_back(arr[i][j].get<int>());
            }
            values.push_back(std::move(inner));
          } else {
            values.push_back(std::nullopt);
          }
        }
      }
      return values;
    };
    auto parse_optional_array = [&](const char* key) {
      std::vector<std::optional<int>> values;
      if (json.contains(key) && json[key].is_array()) {
        const auto& arr = json[key];
        values.reserve(arr.size());
        for (std::size_t i = 0; i < arr.size(); ++i) {
          values.push_back(arr[i].is_null() ? std::optional<int>() : std::optional<int>(arr[i].get<int>()));
        }
      }
      return values;
    };
    auto parse_optional_string_array = [&](const char* key) {
      std::vector<std::optional<std::string>> values;
      if (json.contains(key) && json[key].is_array()) {
        const auto& arr = json[key];
        values.reserve(arr.size());
        for (std::size_t i = 0; i < arr.size(); ++i) {
          values.push_back(arr[i].is_null() ? std::optional<std::string>()
                                            : std::optional<std::string>(arr[i].get<std::string>()));
        }
      }
      return values;
    };
    auto parse_bool_array = [&](const char* key) {
      std::vector<bool> values;
      if (json.contains(key) && json[key].is_array()) {
        const auto& arr = json[key];
        values.reserve(arr.size());
        for (std::size_t i = 0; i < arr.size(); ++i) {
          values.push_back(arr[i].get<bool>());
        }
      }
      return values;
    };

    q.root_degrees = parse_int_array("root_degrees");
    if (q.root_degrees.empty()) {
      q.root_degrees.push_back(json.contains("root_degree") ? json["root_degree"].get<int>() : 0);
    }
    q.qualities = parse_quality_array();
    if (q.qualities.empty()) {
      q.qualities.push_back(json.contains("quality") && json["quality"].is_string()
                                ? triad_quality_from_string(json["quality"].get<std::string>())
                                : TriadQuality::Major);
    }
    std::size_t len = q.root_degrees.size();
    auto ensure_size = [&](auto&& vec, auto default_value) {
      using Vec = std::decay_t<decltype(vec)>;
      Vec out = std::forward<decltype(vec)>(vec);
      if (out.size() < len) {
        out.resize(len, default_value);
      }
      return out;
    };
    q.rh_degrees = ensure_size(parse_optional_vec_array("rh_degrees"), std::optional<std::vector<int>>{});
    q.bass_degrees = ensure_size(parse_optional_array("bass_degrees"), std::optional<int>{});
    q.right_voicing_ids = ensure_size(parse_optional_string_array("right_voicing_id"), std::optional<std::string>{});
    q.bass_voicing_ids = ensure_size(parse_optional_string_array("bass_voicing_id"), std::optional<std::string>{});
    q.is_anchor = ensure_size(parse_bool_array("is_anchor"), false);
    return q;
  }
  if (type == "melody") {
    MelodyQuestionV2 q{};
    q.tonic_midi = json.contains("tonic_midi") ? json["tonic_midi"].get<int>() : 0;
    q.tonic = json.contains("tonic") && json["tonic"].is_string() ? json["tonic"].get<std::string>() : std::string();
    q.key = json.contains("key") && json["key"].is_string() ? key_quality_from_string(json["key"].get<std::string>())
                                                              : KeyQuality::Major;
    q.melody.clear();
    if (json.contains("melody") && json["melody"].is_array()) {
      const auto& arr = json["melody"];
      q.melody.reserve(arr.size());
      for (std::size_t i = 0; i < arr.size(); ++i) {
        q.melody.push_back(arr[i].get<int>());
      }
    }
    if (json.contains("octave") && json["octave"].is_array()) {
      const auto& arr = json["octave"];
      std::vector<int> values;
      values.reserve(arr.size());
      for (std::size_t i = 0; i < arr.size(); ++i) {
        values.push_back(arr[i].get<int>());
      }
      q.octave = std::move(values);
    }
    if (json.contains("helper") && !json["helper"].is_null()) {
      q.helper = json["helper"].get<std::string>();
    }
    return q;
  }
  if (type == "harmony") {
    HarmonyQuestionV2 q{};
    q.tonic_midi = json.contains("tonic_midi") ? json["tonic_midi"].get<int>() : 0;
    q.tonic = json.contains("tonic") && json["tonic"].is_string() ? json["tonic"].get<std::string>() : std::string();
    q.key = json.contains("key") && json["key"].is_string() ? key_quality_from_string(json["key"].get<std::string>())
                                                              : KeyQuality::Major;
    q.note_num = json.contains("note_num") ? json["note_num"].get<int>() : 0;
    q.notes.clear();
    if (json.contains("notes") && json["notes"].is_array()) {
      const auto& arr = json["notes"];
      q.notes.reserve(arr.size());
      for (std::size_t i = 0; i < arr.size(); ++i) {
        q.notes.push_back(arr[i].get<int>());
      }
    }
    if (json.contains("interval") && !json["interval"].is_null()) {
      q.interval = json["interval"].get<std::string>();
    }
    return q;
  }
  throw std::runtime_error("Unknown QuestionPayloadV2 type: " + type);
}

static AnswerPayloadV2 answer_payload_v2_from_json(const nlohmann::json& json) {
  const std::string type = json["type"].get<std::string>();
  if (type == "chord") {
    ChordAnswerV2 a{};
    auto parse_int_array = [&](const char* key) {
      std::vector<int> values;
      if (json.contains(key) && json[key].is_array()) {
        const auto& arr = json[key];
        values.reserve(arr.size());
        for (std::size_t i = 0; i < arr.size(); ++i) {
          values.push_back(arr[i].get<int>());
        }
      }
      return values;
    };
    auto parse_optional_array = [&](const char* key, std::size_t target) {
      std::vector<std::optional<int>> values;
      if (json.contains(key) && json[key].is_array()) {
        const auto& arr = json[key];
        values.reserve(arr.size());
        for (std::size_t i = 0; i < arr.size(); ++i) {
          values.push_back(arr[i].is_null() ? std::optional<int>() : std::optional<int>(arr[i].get<int>()));
        }
      }
      if (!values.empty() && values.size() != target) {
        values.resize(target);
      }
      return values;
    };
    auto parse_bool_array = [&](const char* key, std::size_t target) {
      std::vector<bool> values;
      if (json.contains(key) && json[key].is_array()) {
        const auto& arr = json[key];
        values.reserve(arr.size());
        for (std::size_t i = 0; i < arr.size(); ++i) {
          values.push_back(arr[i].get<bool>());
        }
      }
      if (!values.empty() && values.size() != target) {
        values.resize(target, true);
      }
      return values;
    };

    if (json.contains("root_degrees")) {
      a.root_degrees = parse_int_array("root_degrees");
    } else {
      int single_root = json.contains("root_degree") ? json["root_degree"].get<int>() : 0;
      a.root_degrees.push_back(single_root);
    }
    std::size_t len = a.root_degrees.size();
    if (len == 0) {
      a.root_degrees.push_back(0);
      len = 1;
    }
    auto opt_or_default = [&](std::vector<std::optional<int>> values) {
      if (values.empty()) {
        values.resize(len);
      }
      return values;
    };
    a.bass_deg = opt_or_default(parse_optional_array("bass_deg", len));
    a.top_deg = opt_or_default(parse_optional_array("top_deg", len));
    auto ensure_bool = [&](std::vector<bool> values) {
      if (values.empty()) {
        values.resize(len, true);
      }
      return values;
    };
    a.expect_root = ensure_bool(parse_bool_array("expect_root", len));
    a.expect_bass = ensure_bool(parse_bool_array("expect_bass", len));
    a.expect_top = ensure_bool(parse_bool_array("expect_top", len));
    return a;
  }
  if (type == "melody") {
    MelodyAnswerV2 a{};
    a.melody.clear();
    if (json.contains("melody") && json["melody"].is_array()) {
      const auto& arr = json["melody"];
      a.melody.reserve(arr.size());
      for (std::size_t i = 0; i < arr.size(); ++i) {
        a.melody.push_back(arr[i].get<int>());
      }
    }
    return a;
  }
  if (type == "harmony") {
    HarmonyAnswerV2 a{};
    a.notes.clear();
    if (json.contains("notes") && json["notes"].is_array()) {
      const auto& arr = json["notes"];
      a.notes.reserve(arr.size());
      for (std::size_t i = 0; i < arr.size(); ++i) {
        a.notes.push_back(arr[i].get<int>());
      }
    }
    return a;
  }
  throw std::runtime_error("Unknown AnswerPayloadV2 type: " + type);
}

} // namespace

nlohmann::json to_json(const SessionSpec& spec) {
  nlohmann::json json_spec = nlohmann::json::object();
  json_spec["version"] = spec.version;
  json_spec["drill_kind"] = spec.drill_kind;
  json_spec["key"] = spec.key;
  if (spec.tempo_bpm.has_value()) {
    json_spec["tempo_bpm"] = spec.tempo_bpm.value();
  } else {
    json_spec["tempo_bpm"] = nullptr;
  }
  json_spec["n_questions"] = spec.n_questions;
  json_spec["generation"] = spec.generation;

  nlohmann::json assistance = nlohmann::json::object();
  for (const auto& entry : spec.assistance_policy) {
    assistance[entry.first] = entry.second;
  }
  json_spec["assistance_policy"] = assistance;
  json_spec["sampler_params"] = spec.sampler_params;
  nlohmann::json track_levels = nlohmann::json::array();
  for (int level : spec.track_levels) {
    track_levels.push_back(level);
  }
  json_spec["track_levels"] = track_levels;
  json_spec["seed"] = static_cast<std::int64_t>(spec.seed);
  json_spec["adaptive"] = spec.adaptive;
  json_spec["mode"] = to_string(spec.mode);
  json_spec["level_inspect"] = spec.level_inspect;
  json_spec["params"] = params_to_json(spec.params);
  if (spec.inspect_level.has_value()) {
    json_spec["inspect_level"] = spec.inspect_level.value();
  } else {
    json_spec["inspect_level"] = nullptr;
  }
  if (spec.inspect_tier.has_value()) {
    json_spec["inspect_tier"] = spec.inspect_tier.value();
  } else {
    json_spec["inspect_tier"] = nullptr;
  }
  if (spec.lesson.has_value()) {
    json_spec["lesson"] = spec.lesson.value();
  } else {
    json_spec["lesson"] = nullptr;
  }
  return json_spec;
}

SessionSpec session_spec_from_json(const nlohmann::json& json_spec) {
  SessionSpec spec;
  spec.version = json_spec["version"].get<std::string>();
  spec.drill_kind = json_spec["drill_kind"].get<std::string>();
  spec.key = json_spec["key"].get<std::string>();
  if (!json_spec["tempo_bpm"].is_null()) {
    spec.tempo_bpm = json_spec["tempo_bpm"].get<int>();
  }
  spec.n_questions = json_spec["n_questions"].get<int>();
  spec.generation = json_spec["generation"].get<std::string>();
  spec.assistance_policy.clear();
  const auto& assistance = json_spec["assistance_policy"].get_object();
  for (const auto& entry : assistance) {
    spec.assistance_policy[entry.first] = entry.second.get<int>();
  }
  spec.sampler_params =
      json_spec.contains("sampler_params") ? json_spec["sampler_params"] : nlohmann::json::object();
  if (!spec.sampler_params.is_object()) {
    spec.sampler_params = nlohmann::json::object();
  }

  if (json_spec.contains("range") && json_spec["range"].is_array()) {
    const auto& range = json_spec["range"];
    if (range.size() == 2) {
      const auto to_int = [](const nlohmann::json& node, int fallback) -> int {
        if (node.is_number_integer()) {
          return node.get<int>();
        }
        if (node.is_number_float()) {
          return static_cast<int>(std::lround(node.get<double>()));
        }
        return fallback;
      };
      int lower = std::max(0, to_int(range[0], 0));
      int upper = std::min(127, to_int(range[1], 127));
      if (lower > upper) {
        std::swap(lower, upper);
      }
      const int tonic = drills::central_tonic_midi(spec.key);
      const int clamped_tonic = std::clamp(tonic, lower, upper);
      const int below = std::max(0, clamped_tonic - lower);
      const int above = std::max(0, upper - clamped_tonic);
      if (!spec.sampler_params.contains("range_below_semitones")) {
        spec.sampler_params["range_below_semitones"] = below;
      }
      if (!spec.sampler_params.contains("range_above_semitones")) {
        spec.sampler_params["range_above_semitones"] = above;
      }
    }
  }

  if (json_spec.contains("track_levels")) {
    const auto& levels = json_spec["track_levels"];
    if (levels.is_array()) {
      spec.track_levels.clear();
      for (const auto& entry : levels.get_array()) {
        if (entry.is_number_integer()) {
          spec.track_levels.push_back(entry.get<int>());
        }
      }
    }
  }
  spec.seed = static_cast<std::uint64_t>(json_spec["seed"].get<long long>());
  if (json_spec.contains("adaptive")) {
    spec.adaptive = json_spec["adaptive"].get<bool>();
  }
  if (json_spec.contains("mode") && json_spec["mode"].is_string()) {
    try {
      spec.mode = session_mode_from_string(json_spec["mode"].get<std::string>());
    } catch (const std::exception&) {
      spec.mode = spec.adaptive ? SessionMode::Adaptive : SessionMode::Manual;
    }
  } else if (json_spec.contains("level_inspect") && json_spec["level_inspect"].is_boolean() &&
             json_spec["level_inspect"].get<bool>()) {
    spec.mode = SessionMode::LevelInspector;
  } else if (spec.adaptive) {
    spec.mode = SessionMode::Adaptive;
  } else {
    spec.mode = SessionMode::Manual;
  }
  if (json_spec.contains("level_inspect")) {
    spec.level_inspect = json_spec["level_inspect"].get<bool>();
    if (spec.level_inspect) {
      spec.mode = SessionMode::LevelInspector;
    }
  } else {
    spec.level_inspect = (spec.mode == SessionMode::LevelInspector);
  }
  spec.adaptive = (spec.mode == SessionMode::Adaptive);
  if (json_spec.contains("inspect_level") && !json_spec["inspect_level"].is_null()) {
    spec.inspect_level = json_spec["inspect_level"].get<int>();
  }
  if (json_spec.contains("inspect_tier") && !json_spec["inspect_tier"].is_null()) {
    spec.inspect_tier = json_spec["inspect_tier"].get<int>();
  }
  if (json_spec.contains("lesson") && !json_spec["lesson"].is_null()) {
    spec.lesson = json_spec["lesson"].get<int>();
  }
  if (json_spec.contains("params") && json_spec["params"].is_object()) {
    spec.params = params_from_json(spec.drill_kind, json_spec["params"]);
  } else {
    spec.params = DrillParams{};
  }
  return spec;
}

nlohmann::json to_json(const QuestionBundle& bundle) {
  nlohmann::json json_bundle = nlohmann::json::object();
  json_bundle["question_id"] = bundle.question_id;
  json_bundle["question"] = question_payload_v2_to_json(bundle.question);
  json_bundle["correct_answer"] = answer_payload_v2_to_json(bundle.correct_answer);
  if (bundle.prompt_clip.has_value()) {
    json_bundle["prompt_clip"] = to_json(bundle.prompt_clip.value());
  } else {
    json_bundle["prompt_clip"] = nullptr;
  }
  if (bundle.ui_hints.has_value()) {
    nlohmann::json hints = nlohmann::json::object();
    nlohmann::json assists = nlohmann::json::array();
    for (const auto& s : bundle.ui_hints->allowed_assists) { assists.push_back(s); }
    hints["allowed_assists"] = std::move(assists);
    json_bundle["ui_hints"] = std::move(hints);
  } else {
    json_bundle["ui_hints"] = nullptr;
  }
  return json_bundle;
}

QuestionBundle question_bundle_from_json(const nlohmann::json& json_bundle) {
  QuestionBundle bundle;
  bundle.question_id = json_bundle["question_id"].get<std::string>();
  if (json_bundle.contains("question") && json_bundle["question"].is_object()) {
    bundle.question = question_payload_v2_from_json(json_bundle["question"]);
  }
  bundle.correct_answer = answer_payload_v2_from_json(json_bundle["correct_answer"]);
  if (json_bundle.contains("prompt_clip") && !json_bundle["prompt_clip"].is_null()) {
    // minimal: pass through as-is; deserialize to MidiClip if needed by clients
    // bundle.prompt_clip = midi_clip_from_json(json_bundle["prompt_clip"]);
  }
  // ui_hints optional
  return bundle;
}

nlohmann::json to_json(const AssistBundle& bundle) {
  nlohmann::json json_bundle = nlohmann::json::object();
  json_bundle["question_id"] = bundle.question_id;
  json_bundle["kind"] = bundle.kind;
  if (bundle.prompt_clip.has_value()) {
    json_bundle["prompt_clip"] = to_json(bundle.prompt_clip.value());
  } else {
    json_bundle["prompt_clip"] = nullptr;
  }
  return json_bundle;
}

AssistBundle assist_bundle_from_json(const nlohmann::json& json_bundle) {
  AssistBundle bundle;
  bundle.question_id = json_bundle["question_id"].get<std::string>();
  bundle.kind = json_bundle["kind"].get<std::string>();
  if (json_bundle.contains("prompt_clip") && !json_bundle["prompt_clip"].is_null()) {
    // If needed, implement midi_clip_from_json; for now, keep it null or pass-through elsewhere
  }
  return bundle;
}

nlohmann::json to_json(const ResultReport& report) {
  nlohmann::json json_report = nlohmann::json::object();
  json_report["question_id"] = report.question_id;
  json_report["final_answer"] = answer_payload_v2_to_json(report.final_answer);
  json_report["correct"] = report.correct;
  nlohmann::json metrics = nlohmann::json::object();
  metrics["rt_ms"] = report.metrics.rt_ms;
  metrics["attempts"] = report.metrics.attempts;
  metrics["question_count"] = report.metrics.question_count;
  nlohmann::json assists = nlohmann::json::object();
  for (const auto& entry : report.metrics.assists_used) {
    assists[entry.first] = entry.second;
  }
  metrics["assists_used"] = assists;
  if (report.metrics.first_input_rt_ms.has_value()) {
    metrics["first_input_rt_ms"] = report.metrics.first_input_rt_ms.value();
  } else {
    metrics["first_input_rt_ms"] = nullptr;
  }
  json_report["metrics"] = metrics;
  nlohmann::json attempts = nlohmann::json::array();
  for (const auto& attempt : report.attempts) {
    nlohmann::json entry = nlohmann::json::object();
    entry["label"] = attempt.label;
    entry["correct"] = attempt.correct;
    entry["attempts"] = attempt.attempts;
    if (attempt.answer_fragment.has_value()) {
      entry["answer_fragment"] = typed_to_json(attempt.answer_fragment.value());
    } else {
      entry["answer_fragment"] = nullptr;
    }
    if (attempt.expected_fragment.has_value()) {
      entry["expected_fragment"] = typed_to_json(attempt.expected_fragment.value());
    } else {
      entry["expected_fragment"] = nullptr;
    }
    attempts.push_back(std::move(entry));
  }
  json_report["attempts"] = std::move(attempts);
  return json_report;
}

ResultReport result_report_from_json(const nlohmann::json& json_report) {
  ResultReport report;
  report.question_id = json_report["question_id"].get<std::string>();
  report.final_answer = answer_payload_v2_from_json(json_report["final_answer"]);
  report.correct = json_report["correct"].get<bool>();
  const auto& metrics = json_report["metrics"];
  report.metrics.rt_ms = metrics["rt_ms"].get<int>();
  report.metrics.attempts = metrics["attempts"].get<int>();
  if (metrics.contains("question_count")) {
    report.metrics.question_count = metrics["question_count"].get<int>();
  } else {
    report.metrics.question_count = 1;
  }
  report.metrics.assists_used.clear();
  const auto& assists = metrics["assists_used"].get_object();
  for (const auto& entry : assists) {
    report.metrics.assists_used[entry.first] = entry.second.get<int>();
  }
  if (!metrics["first_input_rt_ms"].is_null()) {
    report.metrics.first_input_rt_ms = metrics["first_input_rt_ms"].get<int>();
  }
  report.attempts.clear();
  if (json_report.contains("attempts") && json_report["attempts"].is_array()) {
    for (const auto& item : json_report["attempts"].get_array()) {
      ResultReport::AttemptDetail detail;
      detail.label = item.contains("label") && item["label"].is_string()
                         ? item["label"].get<std::string>()
                         : std::string();
      detail.correct = item.contains("correct") && item["correct"].is_boolean()
                           ? item["correct"].get<bool>()
                           : false;
      detail.attempts = item.contains("attempts") && item["attempts"].is_number_integer()
                            ? item["attempts"].get<int>()
                            : 0;
      if (item.contains("answer_fragment") && !item["answer_fragment"].is_null()) {
        detail.answer_fragment = typed_from_json(item["answer_fragment"]);
      }
      if (item.contains("expected_fragment") && !item["expected_fragment"].is_null()) {
        detail.expected_fragment = typed_from_json(item["expected_fragment"]);
      }
      report.attempts.push_back(std::move(detail));
    }
  }
  return report;
}

nlohmann::json to_json(const SessionSummary& summary) {
  nlohmann::json json_summary = nlohmann::json::object();
  json_summary["session_id"] = summary.session_id;
  json_summary["totals"] = summary.totals;
  json_summary["by_category"] = summary.by_category;
  json_summary["results"] = summary.results;
  return json_summary;
}

SessionSummary session_summary_from_json(const nlohmann::json& json_summary) {
  SessionSummary summary;
  summary.session_id = json_summary["session_id"].get<std::string>();
  summary.totals = json_summary["totals"];
  summary.by_category = json_summary["by_category"];
  summary.results = json_summary["results"];
  return summary;
}

nlohmann::json to_json(const MemoryPackage& package) {
  nlohmann::json json_package = nlohmann::json::object();
  json_package["summary"] = to_json(package.summary);
  if (package.adaptive.has_value()) {
    const auto& adaptive = package.adaptive.value();
    nlohmann::json json_adaptive = nlohmann::json::object();
    json_adaptive["has_score"] = adaptive.has_score;
    json_adaptive["bout_average"] = adaptive.bout_average;
    json_adaptive["graduate_threshold"] = adaptive.graduate_threshold;
    json_adaptive["level_up"] = adaptive.level_up;
    nlohmann::json drills = nlohmann::json::object();
    for (const auto& entry : adaptive.drills) {
      nlohmann::json drill = nlohmann::json::object();
      drill["family"] = entry.second.family;
      if (entry.second.ema_score.has_value()) {
        drill["ema_score"] = entry.second.ema_score.value();
      } else {
        drill["ema_score"] = nullptr;
      }
      drills[entry.first] = std::move(drill);
    }
    json_adaptive["drills"] = std::move(drills);
    if (adaptive.level.has_value()) {
      const auto& level = adaptive.level.value();
      nlohmann::json json_level = nlohmann::json::object();
      json_level["track_index"] = level.track_index;
      json_level["track_name"] = level.track_name;
      json_level["current_level"] = level.current_level;
      if (level.suggested_level.has_value()) {
        json_level["suggested_level"] = level.suggested_level.value();
      } else {
        json_level["suggested_level"] = nullptr;
      }
      json_adaptive["level"] = std::move(json_level);
    } else {
      json_adaptive["level"] = nullptr;
    }
    json_package["adaptive"] = std::move(json_adaptive);
  } else {
    json_package["adaptive"] = nullptr;
  }
  return json_package;
}

nlohmann::json to_json(const PromptPlan& plan) {
  return prompt_plan_to_json_impl(plan);
}

PromptPlan prompt_plan_from_json(const nlohmann::json& json_plan) {
  return prompt_from_json_impl(json_plan);
}

nlohmann::json to_json(const LevelCatalogEntry& entry) {
  nlohmann::json json_entry = nlohmann::json::object();
  json_entry["level"] = entry.level;
  json_entry["tier"] = entry.tier;
  json_entry["label"] = entry.label;
  return json_entry;
}

LevelCatalogEntry level_catalog_entry_from_json(const nlohmann::json& json_entry) {
  LevelCatalogEntry entry;
  entry.level = json_entry["level"].get<int>();
  entry.tier = json_entry["tier"].get<int>();
  entry.label = json_entry["label"].get<std::string>();
  return entry;
}

} // namespace ear::bridge
