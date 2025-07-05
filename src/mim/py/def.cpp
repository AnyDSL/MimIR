#include <pybind11/pybind11.h>

#include <mim/def.h>
#include <mim/world.h>
#include <pybind11/stl.h>


namespace py = pybind11;

namespace mim {

  void init_def(py::module_& m) {
     py::class_<mim::Def, std::unique_ptr<mim::Def,py::nodelete>>(m, "Def");
  }

} // namespace mim
