#include <pybind11/pybind11.h>

#include <mim/util/log.h>
#include <pybind11/stl.h>


namespace py = pybind11;

namespace mim {

void init_log(py::module_& m) {
    py::class_<mim::Log>(m, "Log")
        .def(py::init<mim::Flags&>())
        .def("set", static_cast<Log& (Log::*)(Log::Level)>(&mim::Log::set), py::return_value_policy::reference)
        .def("set_stdout", [](mim::Log &self) -> mim::Log& {
            return self.set(&std::cout);
        }, py::return_value_policy::reference);


    py::enum_<mim::Log::Level>(m, "Level")
    .value("Error", mim::Log::Level::Error)
    .value("Warn", mim::Log::Level::Warn)
    .value("Info", mim::Log::Level::Info)
    .value("Verbose", mim::Log::Level::Verbose)
    .value("Debug", mim::Log::Level::Debug)
    .export_values();
}


} // namespace mim