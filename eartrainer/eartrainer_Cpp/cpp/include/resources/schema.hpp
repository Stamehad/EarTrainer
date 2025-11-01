// include/resources/schema.hpp
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <variant>
#include <nlohmann/json.hpp>
#include <type_traits>

namespace ear {

//---------------------------------------------------------
// Basic type definitions used by all drill schemas
//---------------------------------------------------------

enum class Kind { kInt, kDouble, kBool, kEnum, kIntList, kString };

struct IntRange   { int min, max, step; };
struct RealRange  { double min, max, step; };
struct Choice     { std::string label; int value; };

// The value variant can store any field’s default
using Value = std::variant<int, double, bool, std::string, std::vector<int>>;

struct Field {
    std::string label;
    Kind kind;
    Value def;
    std::optional<IntRange>  ir{};
    std::optional<RealRange> rr{};
    std::vector<Choice>      choices;
    std::string help;
};

//---------------------------------------------------------
// Schema container
//---------------------------------------------------------
struct Schema {
    std::string id;
    int version = 1;
    std::unordered_map<std::string, Field> fields;

    // optional helper for JSON export
    nlohmann::json to_json() const {
        using nlohmann::json;
        json jf = json::object();
        auto kind_str = [](Kind k) {
            switch (k) {
                case Kind::kInt: return "int";
                case Kind::kDouble: return "double";
                case Kind::kBool: return "bool";
                case Kind::kEnum: return "enum";
                case Kind::kIntList: return "int_list";
                case Kind::kString: return "string";
            }
            return "unknown";
        };
        for (auto& [key, f] : fields) {
            json j;
            j["label"] = f.label;
            j["kind"]  = kind_str(f.kind);
            std::visit([&](const auto& v) {
                using V = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<V, std::vector<int>>) {
                    nlohmann::json arr = nlohmann::json::array();
                    for (int x : v) arr.push_back(x);
                    j["default"] = std::move(arr);
                    } else {
                    j["default"] = v; // works for int/double/bool/std::string
                    }
            }, f.def);
            // std::visit([&](const auto& v) {j["default"] = nlohmann::json(v);}, f.def);
            if (f.ir) j["min"] = f.ir->min, j["max"] = f.ir->max, j["step"] = f.ir->step;
            if (f.rr) j["min"] = f.rr->min, j["max"] = f.rr->max, j["step"] = f.rr->step;
            if (!f.choices.empty()) {
                json ch = json::array();
                for (auto& c : f.choices)
                    ch.push_back({{"label", c.label}, {"value", c.value}});
                j["choices"] = ch;
            }
            j["help"] = f.help;
            jf[key] = std::move(j);
        }
        return json{{"id", id}, {"version", version}, {"fields", std::move(jf)}};
    }
};

} // namespace ear