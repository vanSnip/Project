from flask import Flask, render_template, request, jsonify
from flask_socketio import SocketIO
from datetime import datetime, timedelta
import numpy as np
from scipy.stats import norm
import os
from dotenv import load_dotenv
import webbrowser
import threading
from Library_options import get_option_orderbook_view
from alpaca.data.historical import StockHistoricalDataClient
from alpaca.data.requests import StockBarsRequest
from alpaca.data.timeframe import TimeFrame
import sys
from pathlib import Path

# import own library (C++ pybind11 module)
BASE = Path(__file__).resolve().parent
sys.path.append(str(BASE / "C++_libs"))

# from Options_orderbook.Library_options import *  # pure Python version

# Importing all the homegrown Python functions for testing (uncomment if you want to test the pure Python version)
import Equity_options_SVI_lib as library
import USoptions_lib as US_opt  # C++ pybind11 module (alternative name)

load_dotenv()

API_KEY = os.getenv("ALPACA_API_KEY")
SECRET_KEY = os.getenv("ALPACA_SECRET_KEY")
client = StockHistoricalDataClient(API_KEY, SECRET_KEY)

app = Flask(__name__)
socketio = SocketIO(app, async_mode="threading", cors_allowed_origins="*")

# ---------- global state ----------
state_lock = threading.Lock()
underlying = "AAPL"
expiry = "2026-05-15"
rf = 0.02


# ---------- price through alpaca ----------
def get_latest_price(symbol):
    try:
        end_date = datetime.utcnow() - timedelta(minutes=15)
        start_date = end_date - timedelta(days=3)
        request_bars = StockBarsRequest(
            symbol_or_symbols=symbol,
            timeframe=TimeFrame.Minute,
            start=start_date,
            end=end_date,
            limit=100,
        )
        bars = client.get_stock_bars(request_bars)
        df = bars.df.reset_index()
        if df.empty:
            return None
        df = df.sort_values("timestamp", ascending=False)
        return float(df.iloc[0]["close"])
    except Exception as e:
        print("Price fetch error:", e)
        return None


# ---------- build dataframe ----------
def build_data(symbol, expiry):
    price = get_latest_price(symbol) or 265
    # example forward price for testing

    rounded = 5 * round(price / 5)

    df = get_option_orderbook_view(symbol, expiry, price, half=10, step_size=5)

    fill_cols = [
        "call_bid_qty",
        "call_bid_price",
        "call_ask_price",
        "call_ask_qty",
        "put_bid_qty",
        "put_bid_price",
        "put_ask_price",
        "put_ask_qty",
        "call_iv",
        "put_iv",
    ]

    for col in fill_cols:
        if col in df.columns:
            df[col] = df[col].fillna(0)

    df["mid_call"] = (df["call_bid_price"] + df["call_ask_price"]) / 2
    df["mid_put"] = (df["put_bid_price"] + df["put_ask_price"]) / 2
    df["iv"] = df[["call_iv", "put_iv"]].max(axis=1)
    df["iv"] = df["iv"].replace(0, np.nan).fillna(0.2)

    T = max(
        (datetime.strptime(expiry, "%Y-%m-%d") - datetime.utcnow()).days / 365, 1 / 365
    )

    df["call_theo"] = df.apply(
        lambda r: library.bs_call(price, r["strike"], T, rf, r["iv"]), axis=1
    )
    df["put_theo"] = df.apply(
        lambda r: library.bs_put(price, r["strike"], T, rf, r["iv"]), axis=1
    )

    vol_data = {"strike": df["strike"].tolist(), "iv": df["iv"].tolist()}

    return df, price, vol_data


# ---------- websocket loop ----------
def push_loop():
    print("Push loop started")
    while True:
        try:
            with state_lock:
                current_symbol = underlying
                current_expiry = expiry

            df, price, vol = build_data(current_symbol, current_expiry)

            socketio.emit(
                "update",
                {
                    "orderbook": df.to_dict(orient="records"),
                    "price": price,
                    "vol_data": vol,
                    "underlying": current_symbol,
                    "expiry": current_expiry,
                },
            )
        except Exception as e:
            print("Loop error:", e)
        socketio.sleep(1)


# ---------- routes ----------
@app.route("/")
def index():
    return render_template("index.html", underlying=underlying, expiry=expiry)


@app.route("/get_expiries/<symbol>")
def get_expiries(symbol):
    """Return available expiries for a stock (from Library_options)."""
    try:
        price = get_latest_price(symbol) or 265
        rounded = 5 * round(price / 5)
        df = get_option_orderbook_view(
            symbol, expiry=None, strike_price=rounded, half=10, step_size=5
        )
        return jsonify({"expiries": df["expiry"].dropna().unique().tolist()})
    except Exception as e:
        print("Expiry fetch error:", e)
        return jsonify({"expiries": []})


# ---------- socket events ----------
@socketio.on("connect")
def connect():
    print("Client connected")


@socketio.on("change_symbol")
def change_symbol(data):
    global underlying, expiry
    new_symbol = str(data.get("symbol", "")).upper().strip()
    if not new_symbol:
        return
    with state_lock:
        underlying = new_symbol

        # Fetch available expiries
        try:
            df = get_option_orderbook_view(
                new_symbol, expiry=None, half=10, step_size=5
            )
            expiries = df["expiry"].dropna().unique()
            if len(expiries) > 0:
                expiry = sorted(expiries)[0]  # pick earliest expiry
        except Exception as e:
            print("Expiry fetch error:", e)

    print(f"Symbol changed to: {underlying}, expiry set to: {expiry}")


@socketio.on("change_expiry")
def change_expiry(data):
    global expiry
    new_expiry = str(data.get("expiry", "")).strip()
    if not new_expiry:
        return
    with state_lock:
        expiry = new_expiry
    print(f"Expiry changed to: {expiry}")


# ---------- start ----------
if __name__ == "__main__":
    print("Starting server...")
    socketio.start_background_task(push_loop)
    url = "http://127.0.0.1:5050"
    threading.Timer(0.5, lambda: webbrowser.open(url)).start()
    socketio.run(app, host="127.0.0.1", port=5050, debug=False)
