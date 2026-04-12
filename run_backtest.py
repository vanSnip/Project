# run_backtest.py
import argparse
import backtest_engine
from strategies import STRATEGIES


def parse_unknown_args(unknown):
    """Convert --key value pairs into dict"""
    params = {}
    key = None

    for item in unknown:
        if item.startswith("--"):
            key = item[2:]
        else:
            if key is None:
                continue
            try:
                val = float(item)
            except ValueError:
                val = item
            params[key] = val
            key = None

    return params


def run(strategy_name, overrides):
    if strategy_name not in STRATEGIES:
        raise ValueError(f"Unknown strategy: {strategy_name}")

    strat = STRATEGIES[strategy_name]

    # default params
    params = strat["params"]()

    # override from CLI
    params.update(overrides)

    # call correct C++ function dynamically
    engine_func = getattr(backtest_engine, strat["engine_func"])

    result = engine_func(**params)

    return result, params


if __name__ == "__main__":
    parser = argparse.ArgumentParser()

    parser.add_argument("--strategy", type=str, required=True)

    args, unknown = parser.parse_known_args()

    overrides = parse_unknown_args(unknown)

    result, params = run(args.strategy, overrides)

    print("Strategy:", args.strategy)
    print("Params:", params)
    print("Final PnL:", sum(result.pnl))
