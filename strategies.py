def gamma_scalping_params():
    return {
        "N": 1000,
        "S0": 100,
        "K": 100,
        "T": 1.0,
        "implied_vol": 0.2,
        "realized_vol": 0.25,
        "delta_threshold": 0.05,
    }


def mean_reversion_params():
    return {"N": 1000, "S0": 100, "alpha": 0.1}


STRATEGIES = {
    "gamma": {"params": gamma_scalping_params, "engine_func": "run_gamma_scalping"},
    "mean_reversion": {
        "params": mean_reversion_params,
        "engine_func": "run_mean_reversion",
    },
}
