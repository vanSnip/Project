from setuptools import setup, Extension
from pathlib import Path
import pybind11

BASE = Path(__file__).parent
CPP = BASE / "cpp_files"

ext_modules = [
    Extension(
        "Equity_options_SVI_lib",
        [str(CPP / "Equity_options_SVI_lib.cpp")],
        include_dirs=[pybind11.get_include()],
        language="c++",
        extra_compile_args=["-std=c++17"],
    ),
    Extension(
        "USoptions_lib",
        [str(CPP / "PDE_FD.cpp")],
        include_dirs=[pybind11.get_include()],
        language="c++",
        extra_compile_args=["-std=c++17"],
    ),
    Extension(
        "FX_options_SABR_lib",
        [str(CPP / "FX_options_SABR_lib.cpp")],
        include_dirs=[pybind11.get_include()],
        language="c++",
        extra_compile_args=["-std=c++17"],
    ),
]

setup(
    name="quant_cpp",
    ext_modules=ext_modules,
)

print("set up done")
# To build the C++ extensions, run the following command in the terminal:
# Bash:
# python setup.py build_ext --inplace
