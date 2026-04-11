from setuptools import setup, Extension
import pybind11
import sys

ext_modules = [
    Extension(
        "USoptions_lib",
        ["PDE_FD.cpp"],
        include_dirs=[pybind11.get_include()],
        language="c++",
        extra_compile_args=["-std=c++17"],
    ),
]

setup(
    name="USoptions_lib",
    ext_modules=ext_modules,
)
