#pragma once

#include <cassert>
#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>
#include <initializer_list>
#include <sstream>
#include <type_traits>

namespace nlohmann {

class json {
public:
  using object_t = std::map<std::string, json>;
  using array_t = std::vector<json>;
  using string_t = std::string;
  using boolean_t = bool;
  using number_integer_t = std::int64_t;
  using number_float_t = double;
  using variant_t = std::variant<std::nullptr_t, boolean_t, number_integer_t, number_float_t, string_t, array_t, object_t>;

  json() : data_(nullptr) {}
  json(std::nullptr_t) : data_(nullptr) {}
  json(boolean_t value) : data_(value) {}
  json(number_integer_t value) : data_(value) {}
  json(int value) : data_(static_cast<number_integer_t>(value)) {}
  json(unsigned int value) : data_(static_cast<number_integer_t>(value)) {}
  json(number_float_t value) : data_(value) {}
  json(float value) : data_(static_cast<number_float_t>(value)) {}
  json(const char* value) : data_(string_t(value)) {}
  json(const string_t& value) : data_(value) {}
  json(string_t&& value) : data_(std::move(value)) {}
  json(const array_t& value) : data_(value) {}
  json(array_t&& value) : data_(std::move(value)) {}
  json(const object_t& value) : data_(value) {}
  json(object_t&& value) : data_(std::move(value)) {}

  json(std::initializer_list<std::pair<const std::string, json>> init) : data_(object_t{}) {
    object_t obj;
    for (const auto& item : init) {
      obj[item.first] = item.second;
    }
    data_ = std::move(obj);
  }

  static json object() { return json(object_t{}); }
  static json array() { return json(array_t{}); }

  bool is_null() const { return std::holds_alternative<std::nullptr_t>(data_); }
  bool is_boolean() const { return std::holds_alternative<boolean_t>(data_); }
  bool is_number() const { return is_number_integer() || is_number_float(); }
  bool is_number_integer() const { return std::holds_alternative<number_integer_t>(data_); }
  bool is_number_float() const { return std::holds_alternative<number_float_t>(data_); }
  bool is_string() const { return std::holds_alternative<string_t>(data_); }
  bool is_array() const { return std::holds_alternative<array_t>(data_); }
  bool is_object() const { return std::holds_alternative<object_t>(data_); }

  array_t& get_array() {
    ensure_type_array();
    return std::get<array_t>(data_);
  }

  const array_t& get_array() const {
    ensure_type_array();
    return std::get<array_t>(data_);
  }

  object_t& get_object() {
    ensure_type_object();
    return std::get<object_t>(data_);
  }

  const object_t& get_object() const {
    ensure_type_object();
    return std::get<object_t>(data_);
  }

  string_t& get_string() {
    ensure_type_string();
    return std::get<string_t>(data_);
  }

  const string_t& get_string() const {
    ensure_type_string();
    return std::get<string_t>(data_);
  }

  number_integer_t get_number_integer() const {
    if (is_number_integer()) {
      return std::get<number_integer_t>(data_);
    }
    if (is_number_float()) {
      return static_cast<number_integer_t>(std::get<number_float_t>(data_));
    }
    throw std::runtime_error("json value is not a number");
  }

  number_float_t get_number_float() const {
    if (is_number_float()) {
      return std::get<number_float_t>(data_);
    }
    if (is_number_integer()) {
      return static_cast<number_float_t>(std::get<number_integer_t>(data_));
    }
    throw std::runtime_error("json value is not a number");
  }

  boolean_t get_boolean() const {
    if (!is_boolean()) {
      throw std::runtime_error("json value is not a boolean");
    }
    return std::get<boolean_t>(data_);
  }

  json& operator[](const std::string& key) {
    if (!is_object()) {
      if (!is_null()) {
        throw std::runtime_error("json value is not an object");
      }
      data_ = object_t{};
    }
    return std::get<object_t>(data_)[key];
  }

  const json& operator[](const std::string& key) const {
    ensure_type_object();
    auto it = std::get<object_t>(data_).find(key);
    if (it == std::get<object_t>(data_).end()) {
      static const json null_json;
      return null_json;
    }
    return it->second;
  }

  json& operator[](std::size_t idx) {
    ensure_type_array();
    auto& arr = std::get<array_t>(data_);
    if (idx >= arr.size()) {
      throw std::out_of_range("json array index out of range");
    }
    return arr[idx];
  }

  const json& operator[](std::size_t idx) const {
    ensure_type_array();
    const auto& arr = std::get<array_t>(data_);
    if (idx >= arr.size()) {
      throw std::out_of_range("json array index out of range");
    }
    return arr[idx];
  }

  void push_back(const json& value) {
    if (!is_array()) {
      if (!is_null()) {
        throw std::runtime_error("json value is not an array");
      }
      data_ = array_t{};
    }
    std::get<array_t>(data_).push_back(value);
  }

  std::size_t size() const {
    if (is_array()) {
      return std::get<array_t>(data_).size();
    }
    if (is_object()) {
      return std::get<object_t>(data_).size();
    }
    return 0;
  }

  bool empty() const { return size() == 0; }

  bool contains(const std::string& key) const {
    if (!is_object()) { return false; }
    return std::get<object_t>(data_).count(key) > 0;
  }

  json value(const std::string& key, const json& default_value) const {
    if (!is_object()) {
      return default_value;
    }
    const auto& obj = std::get<object_t>(data_);
    auto it = obj.find(key);
    if (it == obj.end()) {
      return default_value;
    }
    return it->second;
  }

  template <typename T>
  T get() const {
    if constexpr (std::is_same<T, string_t>::value) {
      return get_string();
    } else if constexpr (std::is_same<T, boolean_t>::value) {
      return get_boolean();
    } else if constexpr (std::is_integral<T>::value) {
      return static_cast<T>(get_number_integer());
    } else if constexpr (std::is_floating_point<T>::value) {
      return static_cast<T>(get_number_float());
    } else if constexpr (std::is_same<T, array_t>::value) {
      return get_array();
    } else if constexpr (std::is_same<T, object_t>::value) {
      return get_object();
    } else {
      static_assert(sizeof(T) == 0, "Unsupported json::get type");
    }
  }

  std::string dump(int indent = -1) const {
    std::ostringstream oss;
    dump_internal(oss, indent, 0);
    return oss.str();
  }

  static json parse(const std::string& text) {
    throw std::runtime_error("nlohmann::json minimal stub does not implement parse");
  }

  bool operator==(const json& other) const { return data_ == other.data_; }
  bool operator!=(const json& other) const { return !(*this == other); }

private:
  variant_t data_;

  void ensure_type_array() const {
    if (!is_array()) {
      throw std::runtime_error("json value is not an array");
    }
  }

  void ensure_type_object() const {
    if (!is_object()) {
      throw std::runtime_error("json value is not an object");
    }
  }

  void ensure_type_string() const {
    if (!is_string()) {
      throw std::runtime_error("json value is not a string");
    }
  }

  void dump_internal(std::ostringstream& oss, int indent, int level) const {
    const auto write_indent = [&](int lvl) {
      if (indent >= 0) {
        for (int i = 0; i < lvl * indent; ++i) {
          oss << ' ';
        }
      }
    };

    if (is_null()) {
      oss << "null";
    } else if (is_boolean()) {
      oss << (get_boolean() ? "true" : "false");
    } else if (is_number_integer()) {
      oss << get_number_integer();
    } else if (is_number_float()) {
      oss << get_number_float();
    } else if (is_string()) {
      oss << '\"' << escape_string(get_string()) << '\"';
    } else if (is_array()) {
      oss << '[';
      const auto& arr = std::get<array_t>(data_);
      for (std::size_t i = 0; i < arr.size(); ++i) {
        if (i != 0) {
          oss << ',';
        }
        if (indent >= 0) {
          oss << '\n';
          write_indent(level + 1);
        }
        arr[i].dump_internal(oss, indent, level + 1);
      }
      if (indent >= 0 && !arr.empty()) {
        oss << '\n';
        write_indent(level);
      }
      oss << ']';
    } else if (is_object()) {
      oss << '{';
      const auto& obj = std::get<object_t>(data_);
      std::size_t i = 0;
      for (const auto& item : obj) {
        if (i++ != 0) {
          oss << ',';
        }
        if (indent >= 0) {
          oss << '\n';
          write_indent(level + 1);
        }
        oss << '\"' << escape_string(item.first) << '\"' << ':';
        if (indent >= 0) {
          oss << ' ';
        }
        item.second.dump_internal(oss, indent, level + 1);
      }
      if (indent >= 0 && !obj.empty()) {
        oss << '\n';
        write_indent(level);
      }
      oss << '}';
    }
  }

  static std::string escape_string(const std::string& input) {
    std::string result;
    for (char ch : input) {
      switch (ch) {
        case '\\': result += "\\\\"; break;
        case '\"': result += "\\\""; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default: result += ch; break;
      }
    }
    return result;
  }
};

} // namespace nlohmann

