from setuptools import setup, Extension
from pathlib import Path
import pybind11

BASE = Path(__file__).parent
CPP = BASE / "cpp_files"

ext_modules = [
    Extension(
        "library",
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
        [str(CPP / "SABR_model.cpp")],
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
# python C++_libs/setup_us_options_li.py build_ext --inplace
