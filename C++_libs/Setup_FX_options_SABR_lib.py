from setuptools import setup, Extension
import pybind11

ext_modules = [
    Extension(
        "FX_options_SABR_lib",  # must match PYBIND11_MODULE(library, m)
        ["FX_options_SABR_lib.cpp"],  # your C++ file
        include_dirs=[pybind11.get_include()],
        language="c++",
        extra_compile_args=["-std=c++17"],
    ),
]

setup(
    name="FX_options_SABR_lib",
    ext_modules=ext_modules,
)

print("set up done")
