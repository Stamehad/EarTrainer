#include "ear/session_engine.hpp"

#include "json_bridge.hpp"

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

  py::object assist(const std::string& session_id, const std::string& question_id,
                    const std::string& kind) {
    auto assist_bundle = engine_->assist(session_id, question_id, kind);
    return json_to_py(ear::bridge::to_json(assist_bundle));
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

  py::object capabilities() const {
    return json_to_py(engine_->capabilities());
  }

  py::object debug_state(const std::string& session_id) {
    return json_to_py(engine_->debug_state(session_id));
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
      .def("submit_result", &PySessionEngine::submit_result)
      .def("capabilities", &PySessionEngine::capabilities)
      .def("debug_state", &PySessionEngine::debug_state);
}
