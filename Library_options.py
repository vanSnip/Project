# pip install alpaca-py


# Standard libraries
import os
from datetime import datetime, timedelta

from dotenv import load_dotenv

# Data handling & plotting
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

from scipy.stats import norm

# Alpaca historical stock data
from alpaca.data.historical import StockHistoricalDataClient
from alpaca.data.requests import StockBarsRequest
from alpaca.data.timeframe import TimeFrame
from scipy.optimize import curve_fit

import warnings
from scipy.optimize import curve_fit, OptimizeWarning
import numpy as np

import sys
from pathlib import Path

# import own library (C++ pybind11 module)
BASE = Path(__file__).resolve().parent
sys.path.append(str(BASE / "C++_libs"))

# from Options_orderbook.Library_options import *  # pure Python version

# Importing all the homegrown Python functions for testing (uncomment if you want to test the pure Python version)
import Equity_options_SVI_lib as library

# Alpaca historical option data
from alpaca.data.historical.option import OptionHistoricalDataClient
from alpaca.data.requests import OptionLatestQuoteRequest

load_dotenv()  # Load environment variables from .env file

API_KEY = os.getenv("ALPACA_API_KEY")
SECRET_KEY = os.getenv("ALPACA_SECRET_KEY")

# Create client
client = StockHistoricalDataClient(API_KEY, SECRET_KEY)
option_client = OptionHistoricalDataClient(API_KEY, SECRET_KEY)

# stock_bars_request

# options_bars_request


def get_option_quotes(option_symbols):
    # Fetch all quotes in one request
    option_request = OptionLatestQuoteRequest(
        symbol_or_symbols=option_symbols, feed="indicative"
    )

    option_quotes = option_client.get_option_latest_quote(option_request)

    quotes_list = []

    for sym, quote_obj in option_quotes.items():
        # Only include if bid/ask exists
        if quote_obj.bid_price is not None and quote_obj.ask_price is not None:
            # Extract strike from OPRA symbol
            strike = int(quote_obj.symbol[-8:]) / 1000  # last 8 digits
            quotes_list.append(
                {
                    "bid_size": quote_obj.bid_size,
                    "bid_price": quote_obj.bid_price,
                    "strike": strike,
                    "ask_price": quote_obj.ask_price,
                    "ask_size": quote_obj.ask_size,
                }
            )

    # Convert to DataFrame
    df_options = pd.DataFrame(quotes_list)

    # Reorder columns
    df_options = df_options[
        ["strike", "bid_size", "bid_price", "ask_price", "ask_size"]
    ]

    # Sort by strike
    df_options = df_options.sort_values(by="strike").reset_index(drop=True)

    return df_options


from datetime import datetime


def make_option_symbol(underlying, expiry_date, strike, option_type="C"):
    """
    Build Alpaca/OPRA option symbol.

    Parameters:
        underlying (str): e.g., "AAPL"
        expiry_date (datetime or str): e.g., "2026-03-20" or datetime object
        strike (float or int): strike price
        option_type (str): "C" or "P"

    Returns:
        str: OPRA-style option symbol
    """
    if isinstance(expiry_date, datetime):
        expiry_str = expiry_date.strftime("%y%m%d")
    else:
        expiry_str = datetime.strptime(expiry_date, "%Y-%m-%d").strftime("%y%m%d")

    # OPRA strike: multiply by 1000, no decimals, pad to 8 digits
    strike_int = int(round(strike * 1000))
    strike_str = f"{strike_int:08d}"

    return f"{underlying}{expiry_str}{option_type}{strike_str}"


# to find market price and generate orderbook around it


def generate_option_symbols_around_price(
    underlying, expiry_date, current_price, option_type="C", half=8, step_size=5
):
    """
    Generate a list of OPRA option symbols around a current stock price.

    Parameters:
        underlying (str): e.g., "AAPL"
        expiry_date (str or datetime): e.g., "2026-03-20"
        current_price (float): current stock price
        option_type (str): "C" for calls, "P" for puts
        n_values (int): total number of strikes
        step_size (int): increment between strikes

    Returns:
        list of str: option symbols
    """
    n_values = half * 2 + 1  # Total strikes: half below, half above, plus current
    # Generate strike prices: some below, some above, including current
    strikes = [current_price + (i - half) * step_size for i in range(n_values)]
    # Round to nearest 0.5 or 1 depending on your convention
    strikes = [round(s) for s in strikes]

    # Build option symbols
    symbols = [
        make_option_symbol(underlying, expiry_date, strike, option_type)
        for strike in strikes
    ]
    return symbols


def get_time_to_expiry(expiry):
    expiry_date = datetime.strptime(expiry, "%Y-%m-%d")
    now = datetime.utcnow()
    return max((expiry_date - now).days / 365.0, 0.0001)


def fit_svi(df, S, T, r):
    """
    Fit SVI parameters to market option IVs.

    Parameters
    ----------
    df : pd.DataFrame
        Must contain 'strike' and 'iv' columns
    S : float
        Current stock price
    T : float
        Time to expiry in years
    r : float
        Risk-free rate

    Returns
    -------
    tuple or None
        Fitted SVI parameters: (a, b, rho, m, sigma)
        Returns None if fitting fails or insufficient data
    """
    df = df.copy()
    F = S * np.exp(r * T)  # Forward price

    # Compute log-moneyness and total variance
    df["k"] = np.log(df["strike"] / F)
    df["w"] = (df["iv"] ** 2) * T

    # Remove invalid or duplicate data
    df = df.replace([np.inf, -np.inf], np.nan).dropna(subset=["k", "w"])
    df = df.groupby("k", as_index=False).mean()

    k = df["k"].values.astype(float)
    w = df["w"].values.astype(float)

    if len(k) < 5:
        return None  # Not enough data to fit

    # Initial guess: anchor at ATM
    atm_idx = np.argmin(np.abs(k))
    atm_w = w[atm_idx]
    b_guess = (max(w) - min(w)) / (max(k) - min(k))  # slope
    initial_guess = [atm_w, b_guess, -0.3, 0.0, 0.2]

    # Bounds for stability
    lower_bounds = [0.0, 0.0, -0.999, min(k) - 1, 0.001]
    upper_bounds = [5.0, 5.0, 0.999, max(k) + 1, 5.0]

    try:
        with warnings.catch_warnings():
            warnings.simplefilter("ignore", OptimizeWarning)
            params, _ = curve_fit(
                library.svi_total_variance,
                k,
                w,
                p0=initial_guess,
                bounds=(lower_bounds, upper_bounds),
                maxfev=500000,
            )
        print("SVI fitted:", params)
        return params
    except Exception as e:
        print("SVI fit failed:", e)
        return None


# Black-Scholes formulas for call and put theoretical prices

# -- final option orderbook view function --
from scipy.optimize import brentq
from scipy.stats import norm
import numpy as np


def implied_vol(option_price, S, K, T, r, option_type="call"):
    if option_price <= 0:
        return np.nan

    def objective(sigma):
        d1 = (np.log(S / K) + (r + 0.5 * sigma**2) * T) / (sigma * np.sqrt(T))
        d2 = d1 - sigma * np.sqrt(T)
        if option_type == "call":
            price = S * library.norm_cdf(d1) - K * np.exp(-r * T) * library.norm_cdf(d2)
        else:
            price = K * np.exp(-r * T) * library.norm._cdf(-d2) - S * library.norm_cdf(
                -d1
            )
        return price - option_price

    try:
        return float(brentq(objective, 0.0001, 5.0))
    except:
        return np.nan


def get_option_orderbook_view(
    underlying,
    expiry,
    current_price,
    half=8,
    step_size=5,
    r=0.04,
):
    """
    Generate a full option orderbook with market-derived IVs and SVI-smoothed vols.

    Parameters
    ----------
    underlying : str
        Stock symbol, e.g. "AAPL"
    expiry : str
        Expiry date, e.g. "2026-03-20"
    current_price : float
        Current underlying price
    half : int
        Number of strikes above/below ATM to include
    step_size : float
        Strike interval
    r : float
        Risk-free rate

    Returns
    -------
    pd.DataFrame
        Option orderbook with calls, puts, IVs, and theoretical prices
    """

    #  Time to expiry
    T = get_time_to_expiry(expiry)

    #  Round current price for strike generation
    S = round(current_price / step_size) * step_size

    #  Generate option symbols
    call_symbols = generate_option_symbols_around_price(
        underlying, expiry, S, option_type="C", half=half, step_size=step_size
    )
    put_symbols = generate_option_symbols_around_price(
        underlying, expiry, S, option_type="P", half=half, step_size=step_size
    )

    #  Fetch quotes
    df_calls = get_option_quotes(call_symbols)
    df_puts = get_option_quotes(put_symbols)

    if df_calls.empty or df_puts.empty:
        return pd.DataFrame()  # no quotes

    #  Compute mid prices
    df_calls["mid"] = (df_calls["bid_price"] + df_calls["ask_price"]) / 2
    df_puts["mid"] = (df_puts["bid_price"] + df_puts["ask_price"]) / 2

    #  Filter OTM options
    calls_otm = df_calls[df_calls["strike"] >= current_price].copy()
    puts_otm = df_puts[df_puts["strike"] <= current_price].copy()

    calls_otm = calls_otm[calls_otm["mid"] > 0.01]
    puts_otm = puts_otm[puts_otm["mid"] > 0.01]

    # Compute market IVs
    calls_otm["iv"] = calls_otm.apply(
        lambda row: implied_vol(row["mid"], current_price, row["strike"], T, r, "call"),
        axis=1,
    )
    puts_otm["iv"] = puts_otm.apply(
        lambda row: implied_vol(row["mid"], current_price, row["strike"], T, r, "put"),
        axis=1,
    )

    # Combine OTM vols for SVI fitting
    df_vol = pd.concat(
        [calls_otm[["strike", "iv"]], puts_otm[["strike", "iv"]]]
    ).dropna()
    df_vol = df_vol.sort_values("strike").reset_index(drop=True)
    if len(df_vol) < 5:
        print("Not enough data for SVI fitting, using flat IV")

    print(df_vol)
    print("Trying to fit SVI with", len(df_vol), "points")
    #  Fit SVI
    params = fit_svi(df_vol, current_price, T, r)

    if params is None:
        # fallback: use flat IV from ATM
        atm_iv = df_vol["iv"].iloc[len(df_vol) // 2]  # midpoint
        df_calls["call_iv"] = atm_iv
        df_puts["put_iv"] = atm_iv
    else:
        df_calls["call_iv"] = df_calls["strike"].apply(
            lambda K: library.svi_vol(
                float(K),
                float(current_price),
                float(T),
                float(r),
                float(params[0]),
                float(params[1]),
                float(params[2]),
                float(params[3]),
                float(params[4]),
            )
        )
        df_puts["put_iv"] = df_puts["strike"].apply(
            lambda K: library.svi_vol(
                float(K),
                float(current_price),
                float(T),
                float(r),
                float(params[0]),
                float(params[1]),
                float(params[2]),
                float(params[3]),
                float(params[4]),
            )
        )

    # Compute theoretical prices
    df_calls["call_theo"] = df_calls.apply(
        lambda row: round(
            library.bs_call(current_price, row["strike"], T, r, row["call_iv"]), 2
        ),
        axis=1,
    )
    df_puts["put_theo"] = df_puts.apply(
        lambda row: round(
            library.bs_put(current_price, row["strike"], T, r, row["put_iv"]), 2
        ),
        axis=1,
    )

    # Rename columns for clarity
    df_calls = df_calls.rename(
        columns={
            "bid_price": "call_bid_price",
            "ask_price": "call_ask_price",
            "bid_size": "call_bid_qty",
            "ask_size": "call_ask_qty",
        }
    )
    df_puts = df_puts.rename(
        columns={
            "bid_price": "put_bid_price",
            "ask_price": "put_ask_price",
            "bid_size": "put_bid_qty",
            "ask_size": "put_ask_qty",
        }
    )

    # Merge calls and puts on strike
    df_orderbook = pd.merge(df_calls, df_puts, on="strike", how="outer")

    df_orderbook["iv"] = df_orderbook[["call_iv", "put_iv"]].max(axis=1)
    # Reorder columns
    columns_order = [
        "call_iv",
        "call_bid_qty",
        "call_bid_price",
        "call_theo",
        "call_ask_price",
        "call_ask_qty",
        "strike",
        "put_bid_qty",
        "put_bid_price",
        "put_theo",
        "put_ask_price",
        "put_ask_qty",
        "put_iv",
    ]
    df_orderbook = (
        df_orderbook.reindex(columns=columns_order)
        .sort_values("strike")
        .reset_index(drop=True)
    )
    """
    # --- Add Greeks ---
    df_orderbook["call_delta"] = df_orderbook.apply(
        lambda r: bs_delta_call(current_price, r["strike"], T, r, r["call_iv"]), axis=1
    )
    df_orderbook["put_delta"] = df_orderbook.apply(
        lambda r: bs_delta_put(current_price, r["strike"], T, r, r["put_iv"]), axis=1
    )
    df_orderbook["call_theta"] = df_orderbook.apply(
        lambda r: bs_theta_call(current_price, r["strike"], T, r, r["call_iv"]), axis=1
    )
    df_orderbook["put_theta"] = df_orderbook.apply(
        lambda r: bs_theta_put(current_price, r["strike"], T, r, r["put_iv"]), axis=1
    )
    df_orderbook["call_vega"] = df_orderbook.apply(
        lambda r: bs_vega(current_price, r["strike"], T, r, r["call_iv"]), axis=1
    )
    df_orderbook["put_vega"] = df_orderbook.apply(
        lambda r: bs_vega(current_price, r["strike"], T, r, r["put_iv"]), axis=1
    )
    """
    return df_orderbook


print("file loaded")
