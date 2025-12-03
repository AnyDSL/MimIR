#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <mim/util/dbg.h>

namespace py = pybind11;

namespace mim {

void  register_error(py::module_& m) {
    py::register_exception<mim::Error>(m, "MIM_Error");
}

} // namespace mim
