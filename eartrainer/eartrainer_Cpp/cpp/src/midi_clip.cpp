#include "../include/ear/midi_clip.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ear {

namespace {

int clamp_midi_note(int note) {
  return std::clamp(note, 0, 127);
}

int clamp_velocity(int vel) {
  return std::clamp(vel, 0, 127);
}

} // namespace

MidiClipBuilder::MidiClipBuilder(int tempo_bpm, int ppq) {
  clip_.tempo_bpm = tempo_bpm;
  clip_.ppq = ppq;
  clip_.length_ticks = 0;
  clip_.tracks.clear();
  clip_.format = "midi-clip/v1";
}

int MidiClipBuilder::ms_to_ticks(int dur_ms) const {
  const double ticks_per_ms = (static_cast<double>(clip_.tempo_bpm) * static_cast<double>(clip_.ppq)) /
                              60000.0;
  return static_cast<int>(std::lround(dur_ms * ticks_per_ms));
}

int MidiClipBuilder::add_track(const std::string& name, int channel, int program) {
  MidiTrack track;
  track.name = name;
  track.channel = channel;
  track.program = program;
  clip_.tracks.push_back(std::move(track));
  return static_cast<int>(clip_.tracks.size() - 1);
}

void MidiClipBuilder::ensure_track_index(int track_index) const {
  if (track_index < 0 || static_cast<std::size_t>(track_index) >= clip_.tracks.size()) {
    throw std::out_of_range("MidiClipBuilder track index out of range");
  }
}

void MidiClipBuilder::update_length(int candidate) {
  clip_.length_ticks = std::max(clip_.length_ticks, std::max(0, candidate));
}

void MidiClipBuilder::add_event(int track_index, const MidiEvent& event) {
  ensure_track_index(track_index);
  clip_.tracks[static_cast<std::size_t>(track_index)].events.push_back(event);
  update_length(event.t);
}

void MidiClipBuilder::add_note(int track_index, int start_ticks, int dur_ticks, int note,
                               std::optional<int> velocity) {
  ensure_track_index(track_index);
  const int clamped_start = std::max(0, start_ticks);
  const int clamped_dur = std::max(1, dur_ticks);
  const int clamped_note = clamp_midi_note(note);
  const int vel_value = clamp_velocity(velocity.value_or(90));

  MidiEvent on;
  on.t = clamped_start;
  on.type = "note_on";
  on.note = clamped_note;
  on.vel = vel_value;
  add_event(track_index, on);

  MidiEvent off;
  off.t = clamped_start + clamped_dur;
  off.type = "note_off";
  off.note = clamped_note;
  add_event(track_index, off);

  update_length(off.t);
}

void MidiClipBuilder::set_length_ticks(int ticks) {
  clip_.length_ticks = std::max(clip_.length_ticks, std::max(0, ticks));
}

MidiClip MidiClipBuilder::build() const {
  MidiClip result = clip_;
  int max_t = result.length_ticks;
  for (auto& track : result.tracks) {
    std::stable_sort(track.events.begin(), track.events.end(),
                     [](const MidiEvent& lhs, const MidiEvent& rhs) {
                       return lhs.t < rhs.t;
                     });
    for (const auto& ev : track.events) {
      max_t = std::max(max_t, ev.t);
    }
  }
  result.length_ticks = max_t;
  return result;
}

nlohmann::json to_json(const MidiClip& clip) {
  nlohmann::json json_clip = nlohmann::json::object();
  json_clip["format"] = clip.format;
  json_clip["ppq"] = clip.ppq;
  json_clip["tempo_bpm"] = clip.tempo_bpm;
  json_clip["length_ticks"] = clip.length_ticks;
  nlohmann::json tracks = nlohmann::json::array();
  for (const auto& track : clip.tracks) {
    nlohmann::json json_track = nlohmann::json::object();
    json_track["name"] = track.name;
    json_track["channel"] = track.channel;
    json_track["program"] = track.program;
    nlohmann::json events = nlohmann::json::array();
    for (const auto& ev : track.events) {
      nlohmann::json json_event = nlohmann::json::object();
      json_event["t"] = ev.t;
      json_event["type"] = ev.type;
      if (ev.note.has_value()) {
        json_event["note"] = ev.note.value();
      }
      if (ev.vel.has_value()) {
        json_event["vel"] = ev.vel.value();
      }
      if (ev.control.has_value()) {
        json_event["control"] = ev.control.value();
      }
      if (ev.value.has_value()) {
        json_event["value"] = ev.value.value();
      }
      events.push_back(std::move(json_event));
    }
    json_track["events"] = std::move(events);
    tracks.push_back(std::move(json_track));
  }
  json_clip["tracks"] = std::move(tracks);
  return json_clip;
}

} // namespace ear
