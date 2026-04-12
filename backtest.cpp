#include <vector>
#include <cmath>
#include <random>

struct Result
{
    std::vector<double> pnl;
    std::vector<double> spot;
};

// =========================
// Simple GBM
// =========================
std::vector<double> generate_path(int N, double S0, double sigma, double dt)
{
    std::vector<double> path(N);
    path[0] = S0;

    std::mt19937 gen(42);
    std::normal_distribution<> dist(0.0, 1.0);

    for (int i = 1; i < N; ++i)
    {
        double dW = dist(gen) * std::sqrt(dt);
        path[i] = path[i - 1] * std::exp(-0.5 * sigma * sigma * dt + sigma * dW);
    }

    return path;
}

// =========================
// Core Backtest
// =========================
Result run_gamma_scalping(
    int N,
    double S0,
    double K,
    double T,
    double implied_vol,
    double realized_vol,
    double delta_threshold)
{
    double dt = T / N;
    auto path = generate_path(N, S0, realized_vol, dt);

    double pos_spot = 0.0;
    double cash = 0.0;
    double prev_value = 0.0;

    std::vector<double> pnl_series(N);

    double time = 0.0;

    for (int t = 0; t < N; ++t)
    {
        double S = path[t];
        double tau = std::max(T - time, 1e-6);

        // simple proxy for delta (ATM approx)
        double delta = 0.5;

        double total_delta = 2.0 * delta - 1.0 + pos_spot;

        if (std::abs(total_delta) > delta_threshold)
        {
            double hedge = -total_delta;
            pos_spot += hedge;
            cash -= hedge * S;
        }

        double value = pos_spot * S + cash;

        pnl_series[t] = value - prev_value;
        prev_value = value;

        time += dt;
    }

    return {pnl_series, path};
}