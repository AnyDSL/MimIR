#include <pybind11/pybind11.h>

#include <mim/def.h>
#include <pybind11/stl.h>
#include <mim/lam.h>

namespace py = pybind11;

namespace mim {

void init_app(py::module_& m) {
    py::class_<std::unique_ptr<mim::App, py::nodelete>, mim::Def>(m, "App");
}

} // namespace mim