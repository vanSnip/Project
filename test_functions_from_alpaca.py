from alpaca.data.historical import StockHistoricalDataClient
from alpaca.data.requests import StockBarsRequest, StockQuotesRequest
from alpaca.data.timeframe import TimeFrame

from alpaca.trading.client import TradingClient
from alpaca.trading.requests import MarketOrderRequest, LimitOrderRequest
from alpaca.trading.enums import OrderSide, TimeInForce

from datetime import datetime, timedelta
import os
from dotenv import load_dotenv
import numpy as np

# =========================
# SETUP
# =========================
load_dotenv()

API_KEY = os.getenv("ALPACA_API_KEY")
SECRET_KEY = os.getenv("ALPACA_SECRET_KEY")

data_client = StockHistoricalDataClient(API_KEY, SECRET_KEY)
trading_client = TradingClient(API_KEY, SECRET_KEY, paper=True)

SYMBOL = "AAPL"

# Arbitrary stock price (for modeling/testing)
arbitrary_price = 185.50


# =========================
# 1. MARKET DATA
# =========================
def get_stock_bars(
    symbol,
):
    request = StockBarsRequest(
        symbol_or_symbols=[symbol],
        timeframe=TimeFrame.Minute,
        start=datetime.now() - timedelta(days=1),
    )
    bars = data_client.get_stock_bars(request)
    print("\n--- STOCK BARS ---")
    print(bars)


def get_latest_quotes():
    request = StockQuotesRequest(symbol_or_symbols=[SYMBOL])
    quotes = data_client.get_stock_quotes(request)
    print("\n--- STOCK QUOTES ---")
    print(quotes)


# =========================
# 2. ACCOUNT INFO
# =========================
def get_account_info():
    print(trading_client.get_account())


# =========================
# 3. POSITIONS
# =========================
def get_positions():
    positions = trading_client.get_all_positions()
    print("\n--- POSITIONS ---")
    for pos in positions:
        print(pos)


# =========================
# 4. ORDERS
# =========================
def place_market_order():
    order = MarketOrderRequest(
        symbol=SYMBOL, qty=1, side=OrderSide.BUY, time_in_force=TimeInForce.DAY
    )
    response = trading_client.submit_order(order_data=order)
    print("\n--- MARKET ORDER ---")
    print(response)


def place_limit_order():
    order = LimitOrderRequest(
        symbol=SYMBOL,
        qty=1,
        side=OrderSide.SELL,
        limit_price=arbitrary_price + 5,
        time_in_force=TimeInForce.GTC,
    )
    response = trading_client.submit_order(order_data=order)
    print("\n--- LIMIT ORDER ---")
    print(response)


def place_limit_order_ioc():
    order = LimitOrderRequest(
        symbol=SYMBOL,
        qty=1,
        side=OrderSide.BUY,
        limit_price=arbitrary_price,
        time_in_force=TimeInForce.IOC,
    )
    response = trading_client.submit_order(order_data=order)
    print("\n--- IOC LIMIT ORDER ---")
    print(response)


def place_limit_order_fok():
    order = LimitOrderRequest(
        symbol=SYMBOL,
        qty=1,
        side=OrderSide.BUY,
        limit_price=arbitrary_price,
        time_in_force=TimeInForce.FOK,
    )
    response = trading_client.submit_order(order_data=order)
    print("\n--- FOK LIMIT ORDER ---")
    print(response)


def get_orders():
    orders = trading_client.get_orders()
    print("\n--- ORDERS ---")
    for o in orders:
        print(o)


def try_options_data():
    try:
        from alpaca.data.requests import OptionChainRequest

        request = OptionChainRequest(symbol=SYMBOL)
        chain = data_client.get_option_chain(request)

        print("\n--- OPTION CHAIN ---")
        print(chain)

    except Exception as e:
        print("\nOptions API not available:", e)


try_options_data()
