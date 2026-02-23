#include "frostpack.h"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(frostpack, m) {
    py::class_<frostpack::BitArray2D>(m, "BitArray2D")
        .def(py::init<>())
        .def("sort_key", &frostpack::BitArray2D::sort_key)
        .def_readonly("width", &frostpack::BitArray2D::width)
        .def_readonly("height", &frostpack::BitArray2D::height);

    py::class_<frostpack::Atlas>(m, "Atlas")
        .def(py::init<>())
        .def("place", &frostpack::Atlas::place, py::arg("mask"));

    m.def(
        "raster_island",
        &frostpack::raster_island,
        py::arg("tris"));
}
