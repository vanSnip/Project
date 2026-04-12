#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "backtest.cpp"

namespace py = pybind11;

PYBIND11_MODULE(backtest_engine, m)
{
    py::class_<Result>(m, "Result")
        .def_readwrite("pnl", &Result::pnl)
        .def_readwrite("spot", &Result::spot);

    m.def("run_gamma_scalping", &run_gamma_scalping,
          py::arg("N"),
          py::arg("S0"),
          py::arg("K"),
          py::arg("T"),
          py::arg("implied_vol"),
          py::arg("realized_vol"),
          py::arg("delta_threshold"));
}