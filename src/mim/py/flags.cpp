#include <pybind11/pybind11.h>

#include <mim/flags.h>
#include <pybind11/stl.h>


namespace py = pybind11;

namespace mim {

void init_flags(py::module_& m) {
    py::class_<mim::Flags>(m, "Flags")
        .def(py::init<>())
        .def_readwrite("dump_grid", &mim::Flags::dump_gid)
        .def_readwrite("scalarize_threshhold", &mim::Flags::scalarize_threshold);

}
} // namespace mim