#include <pybind11/detail/common.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include <mim/def.h>
#include <mim/world.h>

namespace py = pybind11;

namespace mim {

void init_def(py::module_& m) {
    // clang-format off
    py::class_<mim::Def, std::unique_ptr<mim::Def, py::nodelete>>(m, "Def")
        .def("var",  py::overload_cast<>(&mim::Def::var), py::return_value_policy::reference_internal)
        .def("proj", py::overload_cast<nat_t, nat_t>(&mim::Def::proj, py::const_), py::return_value_policy::reference_internal)
        .def("proj", py::overload_cast<       nat_t>(&mim::Def::proj, py::const_), py::return_value_policy::reference_internal)
        .def("make_external", &mim::Def::make_external)
        .def("dump", py::overload_cast<>(&mim::Def::dump, py::const_), py::return_value_policy::reference_internal)
        .def("set", static_cast<mim::Def* (mim::Def::*)(std::string)>(&mim::Def::set), py::return_value_policy::reference_internal)
        .def("num_projs", &mim::Def::num_projs, py::return_value_policy::reference_internal)
        .def("projs", [](mim::Def& d, nat_t a) {
            std::vector<const mim::Def*> ret_vec;
            ret_vec.reserve(a);
        
            for (auto* proj : d.projs()) {
                if (ret_vec.size() == a)
                    break;
                ret_vec.push_back(proj);
            }
        
            return ret_vec;
        });

    // clang-format on
}

} // namespace mim
