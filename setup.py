from setuptools import setup, Extension
import pybind11

ext_modules = [
    Extension(
        "library",  # must match PYBIND11_MODULE(library, m)
        ["Library.cpp"],  # your C++ file
        include_dirs=[pybind11.get_include()],
        language="c++",
        extra_compile_args=["-std=c++17"],
    ),
]

setup(
    name="library",
    ext_modules=ext_modules,
)
