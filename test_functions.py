# pip install -e C++_libs
# 3.11.5 base
from matplotlib.style import library
import timeit
import numpy as np
import sys
from pathlib import Path

BASE = Path(__file__).resolve().parent
sys.path.append(str(BASE / "C++_libs"))

# from Options_orderbook.Library_options import *  # pure Python version

# Importing all the homegrown Python functions for testing (uncomment if you want to test the pure Python version)
import Equity_options_SVI_lib as SVI  # C++ pybind11 module
import USoptions_lib as PDE_FD  # C++ pybind11 module (alternative name)
import FX_options_SABR_lib as FXO  # C++ pybind11 module for FX options (SABR model)

# Same inputs for both
S = 100.0
K = 105.0
T = 30 / 365
r = 0.02
sigma = 0.25

sigma_atm = 0.12

# Bloomberg (OVML, FXVO)
RR = -0.01
BF = 0.005

beta = 1.0

alpha, rho, nu = FXO.sabr_calibrate(S, r, T, sigma_atm, RR, BF, beta)
# strikes grid
Ks = np.linspace(0.8, 1.4, 50)
F = S * np.exp(r * T)  # forward price
vols = np.array([FXO.sabr_vol(F, k, T, alpha, 1.0, rho, nu) for k in Ks])

print(alpha, rho, nu)

print(vols)
print("options prices:")
print(SVI.bs_call(S, K, T, r, sigma))
print(PDE_FD.american_fd_call(S, K, T, r, sigma))
print(SVI.bs_put(S, K, T, r, sigma))
print(PDE_FD.american_fd_put(S, K, T, r, sigma))
