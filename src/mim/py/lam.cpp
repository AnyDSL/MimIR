#include <pybind11/pybind11.h>

#include <mim/lam.h>
#include <mim/def.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace mim {

void init_pi(py::module_& m){
    py::class_<mim::Pi>(m, "Pi");
        // .def(py::init<>());
}

void init_lam(py::module_& m) {
    py::class_<mim::Lam>(m, "Lam");
        // .def("app", static_cast<Lam* (Lam::*)(Lam::Filter, const Def*, const Def* )>(&Lam::app));
}

} // namespace mim
