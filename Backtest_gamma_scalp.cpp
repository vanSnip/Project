#include <iostream>
#include <vector>
#include <cmath>
#include <random>

// =========================
// Normal PDF / CDF
// =========================
double norm_pdf(double x)
{
    return std::exp(-0.5 * x * x) / std::sqrt(2.0 * M_PI);
}

double norm_cdf(double x)
{
    return 0.5 * std::erfc(-x / std::sqrt(2));
}

// =========================
// Black-Scholes
// =========================
double bs_price(double S, double K, double T, double r, double sigma, bool is_call)
{
    if (T <= 0.0)
        return std::max(is_call ? (S - K) : (K - S), 0.0);

    double vol_sqrtT = sigma * std::sqrt(T);
    double d1 = (std::log(S / K) + (r + 0.5 * sigma * sigma) * T) / vol_sqrtT;
    double d2 = d1 - vol_sqrtT;

    if (is_call)
        return S * norm_cdf(d1) - K * std::exp(-r * T) * norm_cdf(d2);
    else
        return K * std::exp(-r * T) * norm_cdf(-d2) - S * norm_cdf(-d1);
}

struct Greeks
{
    double delta;
    double gamma;
    double theta;
};

Greeks compute_greeks(double S, double K, double T, double r, double sigma)
{
    Greeks g;

    double vol_sqrtT = sigma * std::sqrt(T);
    double d1 = (std::log(S / K) + (r + 0.5 * sigma * sigma) * T) / vol_sqrtT;
    double d2 = d1 - vol_sqrtT;

    g.delta = norm_cdf(d1); // call delta (use straddle later)

    g.gamma = norm_pdf(d1) / (S * vol_sqrtT);

    g.theta = -(S * norm_pdf(d1) * sigma) / (2.0 * std::sqrt(T)) - r * K * std::exp(-r * T) * norm_cdf(d2);

    return g;
}

// =========================
// GBM Path Generator
// =========================
std::vector<double> generate_path(int N, double S0, double mu, double sigma, double dt)
{
    std::vector<double> path(N);
    path[0] = S0;

    std::mt19937 gen(42);
    std::normal_distribution<> dist(0.0, 1.0);

    for (int i = 1; i < N; ++i)
    {
        double dW = dist(gen) * std::sqrt(dt);
        path[i] = path[i - 1] * std::exp((mu - 0.5 * sigma * sigma) * dt + sigma * dW);
    }

    return path;
}

// =========================
// Gamma Scalper
// =========================
struct GammaScalper
{
    double position_option; // straddle = 2 options
    double position_spot = 0.0;
    double cash = 0.0;

    double delta_threshold = 0.05;
    double transaction_cost = 0.0001;

    void hedge(double S, double delta)
    {
        double total_delta = position_option * delta + position_spot;

        if (std::abs(total_delta) > delta_threshold)
        {
            double hedge_trade = -total_delta;

            position_spot += hedge_trade;
            cash -= hedge_trade * S;

            // transaction cost
            cash -= std::abs(hedge_trade) * S * transaction_cost;
        }
    }
};

// =========================
// Portfolio
// =========================
struct Portfolio
{
    double prev_value = 0.0;

    double value(double S, double opt_price, double pos_opt, double pos_spot, double cash)
    {
        return pos_opt * opt_price + pos_spot * S + cash;
    }
};

// =========================
// MAIN BACKTEST
// =========================
int main()
{
    int N = 1000;
    double S0 = 100.0;
    double K = 100.0;
    double r = 0.0;
    double T = 1.0;

    double implied_vol = 0.2;
    double realized_vol = 0.25;

    double dt = T / N;

    // Generate path
    auto path = generate_path(N, S0, 0.0, realized_vol, dt);

    GammaScalper strat;
    strat.position_option = 2.0; // straddle

    Portfolio port;

    double time = 0.0;

    for (int t = 0; t < N; ++t)
    {
        double S = path[t];
        double T_remaining = std::max(T - time, 1e-6);

        // Call + Put = Straddle
        double call_price = bs_price(S, K, T_remaining, r, implied_vol, true);
        double put_price = bs_price(S, K, T_remaining, r, implied_vol, false);
        double option_price = call_price + put_price;

        Greeks g = compute_greeks(S, K, T_remaining, r, implied_vol);

        // Straddle delta ≈ 2*N(d1)-1
        double straddle_delta = 2.0 * g.delta - 1.0;

        // Hedge
        strat.hedge(S, straddle_delta);

        // Portfolio value
        double value = port.value(S, option_price,
                                  strat.position_option,
                                  strat.position_spot,
                                  strat.cash);

        double pnl = value - port.prev_value;
        port.prev_value = value;

        if (t % 50 == 0)
        {
            std::cout << "t=" << t
                      << " S=" << S
                      << " delta=" << straddle_delta
                      << " gamma=" << g.gamma
                      << " theta=" << g.theta
                      << " pnl=" << pnl
                      << " total=" << value
                      << std::endl;
        }

        time += dt;
    }

    std::cout << "Final PnL: " << port.prev_value << std::endl;

    return 0;
}