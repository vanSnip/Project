from setuptools import setup, Extension
import pybind11
import sys

ext_modules = [
    Extension(
        "library",
        ["Equity_options_SVI_lib.cpp"],
        include_dirs=[pybind11.get_include()],
        language="c++",
        extra_compile_args=["-std=c++17"],
    ),
    Extension(
        "USoptions_lib",
        ["PDE_FD.cpp"],
        include_dirs=[pybind11.get_include()],
        language="c++",
        extra_compile_args=["-std=c++17"],
    ),
    Extension(
        "FX_options_SABR_lib",
        ["SABR_model.cpp"],
        include_dirs=[pybind11.get_include()],
        language="c++",
        extra_compile_args=["-std=c++17"],
    ),
]

setup(
    name="quant_cpp_libs",
    ext_modules=ext_modules,
)

print("set up done")
# To build the C++ extensions, run the following command in the terminal:
# Bash:
# python setup.py build_ext --inplace

# To remove all the built extensions, you can delete the generated .so files in the current directory.
# Bash:
# rm *.so
