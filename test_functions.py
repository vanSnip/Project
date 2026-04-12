from matplotlib.style import library
import timeit
import numpy as np

# from Options_orderbook.Library_options import *  # pure Python version

# Importing all the homegrown Python functions for testing (uncomment if you want to test the pure Python version)
import library  # C++ pybind11 module
import USoptions_lib  # C++ pybind11 module (alternative name)
import FX_options_SABR_lib as sabr  # C++ pybind11 module for FX options (SABR model)

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

alpha, rho, nu = sabr.sabr_calibrate(S, r, T, sigma_atm, RR, BF, beta)
# strikes grid
K = np.linspace(0.8, 1.4, 50)
F = S * np.exp(r * T)  # forward price
vols = np.array([sabr.sabr_vol(F, k, T, alpha, 1.0, rho, nu) for k in K])

print(alpha, rho, nu)

""" 

print(vols)
print(library.bs_call(S, K, T, r, sigma))
print(USoptions_lib.american_fd_call(S, K, T, r, sigma))
print(library.bs_put(S, K, T, r, sigma))
print(USoptions_lib.american_fd_put(S, K, T, r, sigma))

"""
