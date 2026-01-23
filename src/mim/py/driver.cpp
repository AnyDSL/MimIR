#include <fe/sym.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>

#include <mim/driver.h>
#include <mim/ast/ast.h>
namespace py = pybind11;

namespace mim {

void init_driver(py::module_& m) {
    py::class_<mim::Driver, std::unique_ptr<mim::Driver, py::nodelete>, fe::SymPool>(m, "Driver")
        .def(py::init<>())
        .def("world", &mim::Driver::world, py::return_value_policy::reference_internal)
        .def(
            "add_import",
            [](mim::Driver& driver, std::string path, std::string str) {
                return driver.imports().add(fs::path(path), driver.sym(str));
            },
            py::return_value_policy::reference_internal)
        .def("add_search_path", &mim::Driver::add_search_path)
        .def("log", &mim::Driver::log, py::return_value_policy::reference_internal)
        .def("backend", [](mim::Driver& d, std::string backend, std::string output_file_name, mim::World& world) {
            std::ofstream ofs(output_file_name);
            d.backend(backend)(world, ofs);
            ofs.close();
        })
        .def("load_pluins", [](mim::Driver& d, std::vector<std::string> plugins){
            ast::load_plugins(d.world(), plugins);
        });
}

} // namespace mim
