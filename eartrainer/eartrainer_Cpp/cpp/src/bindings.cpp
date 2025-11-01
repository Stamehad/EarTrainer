#include "../include/ear/session_engine.hpp"
#include "../include/ear/adaptive_drills.hpp"

#include "json_bridge.hpp"
#include "../include/ear/midi_clip.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace {

nlohmann::json py_to_json(py::handle handle) {
  if (handle.is_none()) {
    return nullptr;
  }
  if (py::isinstance<py::bool_>(handle)) {
    return handle.cast<bool>();
  }
  if (py::isinstance<py::int_>(handle)) {
    return static_cast<long long>(handle.cast<long long>());
  }
  if (py::isinstance<py::float_>(handle)) {
    return handle.cast<double>();
  }
  if (py::isinstance<py::str>(handle)) {
    return handle.cast<std::string>();
  }
  if (py::isinstance<py::dict>(handle)) {
    nlohmann::json json_obj = nlohmann::json::object();
    for (auto item : handle.cast<py::dict>()) {
      auto key = py::cast<std::string>(item.first);
      json_obj[key] = py_to_json(item.second);
    }
    return json_obj;
  }
  if (py::isinstance<py::list>(handle) || py::isinstance<py::tuple>(handle)) {
    nlohmann::json json_array = nlohmann::json::array();
    for (auto item : handle.cast<py::sequence>()) {
      json_array.push_back(py_to_json(item));
    }
    return json_array;
  }
  throw std::runtime_error("Unsupported Python type for JSON conversion");
}

py::object json_to_py(const nlohmann::json& json_value) {
  if (json_value.is_null()) {
    return py::none();
  }
  if (json_value.is_boolean()) {
    return py::bool_(json_value.get<bool>());
  }
  if (json_value.is_number_integer()) {
    return py::int_(json_value.get<long long>());
  }
  if (json_value.is_number_float()) {
    return py::float_(json_value.get<double>());
  }
  if (json_value.is_string()) {
    return py::str(json_value.get<std::string>());
  }
  if (json_value.is_array()) {
    py::list list;
    for (const auto& element : json_value.get_array()) {
      list.append(json_to_py(element));
    }
    return list;
  }
  if (json_value.is_object()) {
    py::dict dict;
    for (const auto& entry : json_value.get_object()) {
      dict[py::str(entry.first)] = json_to_py(entry.second);
    }
    return dict;
  }
  throw std::runtime_error("Unhandled JSON type");
}

class PySessionEngine {
public:
  PySessionEngine() : engine_(ear::make_engine()) {}

  std::string create_session(py::object spec_obj) {
    auto spec_json = py_to_json(spec_obj);
    auto spec = ear::bridge::session_spec_from_json(spec_json);
    return engine_->create_session(spec);
  }

  py::object next_question(const std::string& session_id) {
    auto next = engine_->next_question(session_id);
    if (auto bundle = std::get_if<ear::QuestionBundle>(&next)) {
      return json_to_py(ear::bridge::to_json(*bundle));
    }
    return json_to_py(ear::bridge::to_json(std::get<ear::SessionSummary>(next)));
  }

  py::object assist(const std::string& session_id, const std::string& kind) {
    auto assist_bundle = engine_->assist(session_id, kind);
    return json_to_py(ear::bridge::to_json(assist_bundle));
  }

  py::list assist_options(const std::string& session_id) {
    auto options = engine_->assist_options(session_id);
    py::list result;
    for (const auto& kind : options) {
      result.append(kind);
    }
    return result;
  }

  void set_level(const std::string& session_id, int level, int tier) {
    engine_->set_level(session_id, level, tier);
  }

  std::string level_catalog_overview(const std::string& session_id) {
    return engine_->level_catalog_overview(session_id);
  }

  std::string level_catalog_levels(const std::string& session_id) {
    return engine_->level_catalog_levels(session_id);
  }

  py::object level_catalog_entries(py::object spec_obj) {
    auto spec_json = py_to_json(spec_obj);
    auto spec = ear::bridge::session_spec_from_json(spec_json);
    auto entries = engine_->level_catalog_entries(spec);
    nlohmann::json json_entries = nlohmann::json::array();
    for (const auto& entry : entries) {
      json_entries.push_back(ear::bridge::to_json(entry));
    }
    return json_to_py(json_entries);
  }

  py::object submit_result(const std::string& session_id, py::object report_obj) {
    auto report_json = py_to_json(report_obj);
    auto report = ear::bridge::result_report_from_json(report_json);
    auto next = engine_->submit_result(session_id, report);
    if (auto bundle = std::get_if<ear::QuestionBundle>(&next)) {
      return json_to_py(ear::bridge::to_json(*bundle));
    }
    return json_to_py(ear::bridge::to_json(std::get<ear::SessionSummary>(next)));
  }

  std::string session_key(const std::string& session_id) {
    return engine_->session_key(session_id);
  }

  py::object orientation_prompt(const std::string& session_id) {
    auto clip = engine_->orientation_prompt(session_id);
    return json_to_py(ear::to_json(clip));
  }

  py::object capabilities() const {
    return json_to_py(engine_->capabilities());
  }

  py::object drill_param_spec() const {
    return json_to_py(engine_->drill_param_spec());
  }

  py::object debug_state(const std::string& session_id) {
    return json_to_py(engine_->debug_state(session_id));
  }

  py::object adaptive_diagnostics(const std::string& session_id) {
    return json_to_py(engine_->adaptive_diagnostics(session_id));
  }

  py::object end_session(const std::string& session_id) {
    auto package = engine_->end_session(session_id);
    return json_to_py(ear::bridge::to_json(package));
  }

private:
  std::unique_ptr<ear::SessionEngine> engine_;
};

} // namespace

PYBIND11_MODULE(_earcore, m) {
  py::class_<PySessionEngine>(m, "SessionEngine")
      .def(py::init<>())
      .def("create_session", &PySessionEngine::create_session)
      .def("next_question", &PySessionEngine::next_question)
      .def("assist", &PySessionEngine::assist)
      .def("assist_options", &PySessionEngine::assist_options)
      .def("set_level", &PySessionEngine::set_level)
      .def("level_catalog_overview", &PySessionEngine::level_catalog_overview)
      .def("level_catalog_levels", &PySessionEngine::level_catalog_levels)
      .def("level_catalog_entries", &PySessionEngine::level_catalog_entries)
      .def("submit_result", &PySessionEngine::submit_result)
      .def("session_key", &PySessionEngine::session_key)
      .def("orientation_prompt", &PySessionEngine::orientation_prompt)
      .def("end_session", &PySessionEngine::end_session)
      .def("capabilities", &PySessionEngine::capabilities)
      .def("drill_param_spec", &PySessionEngine::drill_param_spec)
      .def("debug_state", &PySessionEngine::debug_state)
      .def("adaptive_diagnostics", &PySessionEngine::adaptive_diagnostics);

  py::class_<ear::AdaptiveDrills>(m, "AdaptiveDrills")
      .def(py::init<std::string, std::uint64_t>(),
           py::arg("resources_dir") = std::string("eartrainer/eartrainer_Cpp/resources"),
           py::arg("seed") = 1)
      .def("set_bout",
           [](ear::AdaptiveDrills& ad, const std::vector<int>& levels) {
             ad.set_bout(levels);
           },
           py::arg("levels"))
      .def("set_bout",
           [](ear::AdaptiveDrills& ad, int level) {
             ad.set_bout(std::vector<int>{level});
           },
           py::arg("level"))
      .def("next", [](ear::AdaptiveDrills& ad) {
        auto b = ad.next();
        return json_to_py(ear::bridge::to_json(b));
      })
      .def("diagnostic", [](const ear::AdaptiveDrills& ad) {
        return json_to_py(ad.diagnostic());
      })
      .def("size", &ear::AdaptiveDrills::size)
      .def("empty", &ear::AdaptiveDrills::empty);
}
