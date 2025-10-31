// param_schema.hpp
#pragma once
#include <string>
#include <vector>
#include <functional>
#include <optional>
#include <type_traits>
#include <nlohmann/json.hpp>

namespace ear {

// --- UI kinds ---
enum class UIKind { Int, Double, Bool, Enum, IntList, String, BoolOpt, StringOpt, Nested };

// --- enum choice ---
struct Choice { std::string label; int value; };

// --- Forward decl for nested schemas ---
template <typename T>
struct Schema;

// --- FieldSpec<T>: one field descriptor for a specific struct T ---
template <typename T>
struct FieldSpec {
  std::string key;              // "tempo_bpm"
  std::string label;            // "Tempo (BPM)"
  UIKind kind;
  // Emit default + UI metadata from a default-constructed T
  std::function<void(const T&, nlohmann::json& fields)> emit;
  // Parse a single field from a JSON object into a mutable T
  std::function<void(const nlohmann::json& obj, T&, std::vector<std::string>& errs)> parse;
};

// --- Schema<T>: a list of FieldSpec<T> + helpers ---
template <typename T>
struct Schema {
  std::string id;       // "interval_params"
  int version = 1;
  std::vector<FieldSpec<T>> fields;

  nlohmann::json to_json(const T& defaults = T{}) const {
    nlohmann::json out;
    out["id"] = id;
    out["version"] = version;
    nlohmann::json arr = nlohmann::json::array();
    for (auto& f : fields) f.emit(defaults, arr);
    out["fields"] = std::move(arr);
    return out;
  }

  // Parse JSON object {"tempo_bpm": 90, ...} into T with validation
  std::pair<std::optional<T>, std::vector<std::string>>
  from_json(const nlohmann::json& obj) const {
    T v{}; // start from struct defaults
    std::vector<std::string> errs;
    for (auto& f : fields) f.parse(obj, v, errs);
    if (!errs.empty()) return {std::nullopt, errs};
    return {std::move(v), {}};
  }
};

// ---------- Helpers (build once, reuse everywhere) ----------

template <typename T>
FieldSpec<T> make_int(std::string key, std::string label,
                      int T::* mem, int mn, int mx, int step) {
  return {
    std::move(key), std::move(label), UIKind::Int,
    // emit
    [key,label,mem,mn,mx,step](const T& d, nlohmann::json& fields){
      fields.push_back({{"key",key},{"label",label},{"kind","int"},
                        {"default", d.*mem}, {"min",mn},{"max",mx},{"step",step}});
    },
    // parse
    [key,mem,mn,mx](const nlohmann::json& obj, T& dst, std::vector<std::string>& errs){
      if (!obj.contains(key)) return; // keep default
      if (!obj[key].is_number_integer()) { errs.push_back(key + ": not int"); return; }
      int v = obj[key].get<int>();
      if (v < mn || v > mx) { errs.push_back(key + ": out of range"); return; }
      dst.*mem = v;
    }
  };
}

template <typename T>
FieldSpec<T> make_double(std::string key, std::string label,
                         double T::* mem, double mn, double mx, double step) {
  return {
    key, label, UIKind::Double,
    [key,label,mem,mn,mx,step](const T& d, nlohmann::json& fields){
      fields.push_back({{"key",key},{"label",label},{"kind","double"},
                        {"default", d.*mem}, {"min",mn},{"max",mx},{"step",step}});
    },
    [key,mem,mn,mx](const nlohmann::json& obj, T& dst, std::vector<std::string>& errs){
      if (!obj.contains(key)) return;
      if (!obj[key].is_number()) { errs.push_back(key + ": not number"); return; }
      double v = obj[key].get<double>();
      if (v < mn || v > mx) { errs.push_back(key + ": out of range"); return; }
      dst.*mem = v;
    }
  };
}

template <typename T>
FieldSpec<T> make_bool(std::string key, std::string label, bool T::* mem) {
  return {
    key, label, UIKind::Bool,
    [key,label,mem](const T& d, nlohmann::json& fields){
      fields.push_back({{"key",key},{"label",label},{"kind","bool"},{"default", d.*mem}});
    },
    [key,mem](const nlohmann::json& obj, T& dst, std::vector<std::string>& errs){
      if (!obj.contains(key)) return;
      if (!obj[key].is_boolean()) { errs.push_back(key + ": not bool"); return; }
      dst.*mem = obj[key].get<bool>();
    }
  };
}

template <typename T>
FieldSpec<T> make_optional_bool(std::string key, std::string label, std::optional<bool> T::* mem) {
  return {
    key, label, UIKind::BoolOpt,
    [key,label,mem](const T& d, nlohmann::json& fields){
      nlohmann::json f = {{"key",key},{"label",label},{"kind","bool"}};
      if (d.*mem) f["default"] = *(d.*mem);
      fields.push_back(std::move(f));
    },
    [key,mem](const nlohmann::json& obj, T& dst, std::vector<std::string>& errs){
      if (!obj.contains(key)) return; // leave as std::nullopt
      if (obj[key].is_null()) { dst.*mem = std::nullopt; return; }
      if (!obj[key].is_boolean()) { errs.push_back(key + ": not bool/null"); return; }
      dst.*mem = obj[key].get<bool>();
    }
  };
}

template <typename T>
FieldSpec<T> make_vec_int(std::string key, std::string label, std::vector<int> T::* mem) {
  return {
    key, label, UIKind::IntList,
    [key,label,mem](const T& d, nlohmann::json& fields){
      fields.push_back({{"key",key},{"label",label},{"kind","int_list"},{"default", d.*mem}});
    },
    [key,mem](const nlohmann::json& obj, T& dst, std::vector<std::string>& errs){
      if (!obj.contains(key)) return;
      if (!obj[key].is_array()) { errs.push_back(key + ": not array"); return; }
      std::vector<int> out;
      for (auto& el : obj[key]) {
        if (!el.is_number_integer()) { errs.push_back(key + ": array has non-int"); return; }
        out.push_back(el.get<int>());
      }
      dst.*mem = std::move(out);
    }
  };
}

// Enum as int in JSON
template <typename T, typename Enum>
FieldSpec<T> make_enum(std::string key, std::string label, Enum T::* mem,
                       std::vector<Choice> choices) {
  return {
    key, label, UIKind::Enum,
    [key,label,mem,choices](const T& d, nlohmann::json& fields){
      nlohmann::json ch = nlohmann::json::array();
      for (auto& c : choices) ch.push_back({{"label",c.label},{"value",c.value}});
      fields.push_back({{"key",key},{"label",label},{"kind","enum"},
                        {"default", static_cast<int>(d.*mem)}, {"choices", ch}});
    },
    [key,mem,choices](const nlohmann::json& obj, T& dst, std::vector<std::string>& errs){
      if (!obj.contains(key)) return;
      if (!obj[key].is_number_integer()) { errs.push_back(key + ": not int"); return; }
      int v = obj[key].get<int>();
      bool ok=false; for (auto& c:choices) if (c.value==v) { ok=true; break; }
      if (!ok) { errs.push_back(key + ": invalid enum value"); return; }
      dst.*mem = static_cast<std::remove_reference_t<decltype(dst.*mem)>>(v);
    }
  };
}

// Nested object (e.g., ChordParams::TrainingRootConfig) — emits a nested schema field
template <typename T, typename Nested>
FieldSpec<T> make_nested(std::string key, std::string label, Nested T::* mem,
                         const Schema<Nested>& nested_schema) {
  return {
    key, label, UIKind::Nested,
    [key,label,mem,&nested_schema](const T& d, nlohmann::json& fields){
      auto defaults = d.*mem;
      auto j = nested_schema.to_json(defaults);
      j["key"] = key; j["label"] = label; // for clarity
      fields.push_back(std::move(j));      // client can recurse
    },
    [key,mem,&nested_schema](const nlohmann::json& obj, T& dst, std::vector<std::string>& errs){
      if (!obj.contains(key)) return;
      if (!obj[key].is_object()) { errs.push_back(key + ": not object"); return; }
      auto [val, e] = nested_schema.from_json(obj[key]);
      if (!e.empty()) { for (auto& s: e) errs.push_back(key + "." + s); return; }
      dst.*mem = *val;
    }
  };
}

} // namespace ear