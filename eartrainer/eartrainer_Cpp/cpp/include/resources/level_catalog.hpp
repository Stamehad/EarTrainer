#pragma once
#include "ear/drill_spec.hpp"
#include "resources/drill_params.hpp"
#include <map>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <string>

namespace ear::builtin::catalog_numbered {

// ---------- numbering ----------
inline int tier_of(int n)         { return n % 10; }
inline int block_of(int n)        { return n / 10; }
inline bool is_mixer_block(int n) { return (block_of(n) % 10) == 9; }
inline int promote_of(int n)      { return n + 1; }
inline int demote_of(int n) {
    int t = tier_of(n);
    if (t == 5) return n - 5;
    return (block_of(n) - 1) * 10 + 5;
}

// ---------- manifest ----------
using BuildFn = ear::DrillParams(*)();

struct MetaData{
    int demote_to;
    int promote_to;
    int mix = -1;         // mix drill with this number, -1 means no mix
};
struct DrillEntry {
    int number;
    BuildFn build;
    int q = 10;
    const char* name = nullptr;
};

inline const char* lesson_to_family(int lesson_number);

struct Node{
    int number;
    std::vector<BuildFn> builds;
    const char* name = nullptr;
    MetaData meta{};
};
enum class LessonType {Lesson, Mixer, Warmup};
struct Lesson{
    int lesson;
    std::string name;
    std::vector<DrillEntry> drills;
    LessonType type = LessonType::Lesson;
    MetaData meta{};

    const int get_level() const {
        return (lesson / 10) % 10;
    }

    const char* family() const {
        return lesson_to_family(lesson);
    }
};

// ---------- family ----------
inline const char* family_of(const ear::DrillParams& dp) {
    if (std::holds_alternative<ear::NoteParams>(dp))    return "note";
    if (std::holds_alternative<ear::MelodyParams>(dp))  return "melody";
    if (std::holds_alternative<ear::IntervalParams>(dp))return "interval";
    if (std::holds_alternative<ear::ChordParams>(dp))   return "chord";
    return "none";
}

inline const char* lesson_to_family(int lesson_number) {
    int level = (lesson_number / 100) % 10;
    if (level == 0) return "melody";
    if (level == 1) return "harmony";
    if (level == 2) return "chord";
    return "none";
}

inline DrillSpec make_spec_from_entry(const DrillEntry& e) {
    DrillParams params = e.build();
    DrillSpec s;
    s.id     = std::to_string(e.number);
    s.family = family_of(params);
    s.level  = block_of(e.number);
    s.tier   = tier_of(e.number);
    s.params = std::move(params);
    return s;
}

// ---------- generic adapter ----------
template <class Provider>
struct NumberedCatalog {
    using Entry = DrillEntry;

    static const std::map<int, std::vector<Entry>>& manifest_by_block() {
        static const std::map<int, std::vector<Entry>> idx = []{
            std::map<int, std::vector<Entry>> mp;
            for (const auto& e : Provider::manifest()) mp[block_of(e.number)].push_back(e);
            for (auto& kv : mp) {
                auto& v = kv.second;
                std::sort(v.begin(), v.end(),
                          [](const Entry& a, const Entry& b){ return a.number < b.number; });
            }
            return mp;
        }();
        return idx;
    }

    static const std::vector<int>& known_levels() {
        static const std::vector<int> v = []{
            std::vector<int> out;
            out.reserve(manifest_by_block().size());
            for (auto& kv : manifest_by_block()) out.push_back(kv.first);
            return out;
        }();
        return v;
    }

    static const std::map<int, std::vector<int>>& phases() {
        // If you still want a grouping — adjust if your notion of "phase" changes
        static const std::map<int, std::vector<int>> m = []{
            std::map<int, std::vector<int>> ph;
            for (auto& [block, _] : manifest_by_block()) {
                int phase_digit = block % 10;
                ph[phase_digit].push_back(block);
            }
            for (auto& kv : ph) std::sort(kv.second.begin(), kv.second.end());
            return ph;
        }();
        return m;
    }

    static const std::vector<DrillSpec>& drills_for_level(int level_block) {
        static std::unordered_map<int, std::vector<DrillSpec>> cache;
        auto it = cache.find(level_block);
        if (it != cache.end()) return it->second;

        const auto& mp = manifest_by_block();
        auto f = mp.find(level_block);
        static const std::vector<DrillSpec> empty{};
        if (f == mp.end()) return empty;

        std::vector<DrillSpec> specs;
        specs.reserve(f->second.size());
        for (const auto& e : f->second) specs.push_back(make_spec_from_entry(e));
        auto& stored = cache[level_block] = std::move(specs);
        return stored;
    }
};

} // namespace ear::builtin::catalog_numbered
