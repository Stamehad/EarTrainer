#pragma once

#include <optional>
#include <string>
#include <vector>

#include "../nlohmann/json.hpp"

namespace ear {

struct MidiEvent {
  int t = 0;
  std::string type;
  std::optional<int> note;
  std::optional<int> vel;
  std::optional<int> control;
  std::optional<int> value;
};

struct MidiTrack {
  std::string name;
  int channel = 0;
  int program = 0;
  std::vector<MidiEvent> events;
};

struct MidiClip {
  int ppq = 480;
  int tempo_bpm = 90;
  int length_ticks = 0;
  std::vector<MidiTrack> tracks;
  std::string format = "midi-clip/v1";
};

class MidiClipBuilder {
public:
  explicit MidiClipBuilder(int tempo_bpm, int ppq = 480);

  int tempo_bpm() const { return clip_.tempo_bpm; }
  int ppq() const { return clip_.ppq; }

  int ms_to_ticks(int dur_ms) const;

  int add_track(const std::string& name, int channel, int program);
  void add_event(int track_index, const MidiEvent& event);
  void add_note(int track_index, int start_ticks, int dur_ticks, int note,
                std::optional<int> velocity = std::optional<int>(90));

  void set_length_ticks(int ticks);

  MidiClip build() const;

private:
  void ensure_track_index(int track_index) const;
  void update_length(int candidate);

  MidiClip clip_;
};

nlohmann::json to_json(const MidiClip& clip);

} // namespace ear
