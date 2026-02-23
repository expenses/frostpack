#include "frostpack.h"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(frostpack, m) {
    py::class_<frostpack::BitArray2D>(m, "BitArray2D")
        .def("sort_key", &frostpack::BitArray2D::sort_key)
        .def_readonly("width", &frostpack::BitArray2D::width)
        .def_readonly("height", &frostpack::BitArray2D::height)
        .def_readonly("uv_min", &frostpack::BitArray2D::uv_min)
        .def("get", &frostpack::BitArray2D::get);

    py::class_<frostpack::Atlas>(m, "Atlas")
        .def(py::init<uint32_t>())
        .def("place", &frostpack::Atlas::place, py::arg("mask"))
        .def_readonly("array", &frostpack::Atlas::array);

    m.def("raster_island", &frostpack::raster_island, py::arg("tris"));
}
