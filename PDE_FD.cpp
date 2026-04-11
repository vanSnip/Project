#include <pybind11/pybind11.h>
#include <cmath>
#include <vector>
#include <algorithm>
#include <stdexcept>

namespace py = pybind11;

// ============================================================
// Normal CDF (optional utility)
// ============================================================
double norm_cdf(double x)
{
    return 0.5 * std::erfc(-x / std::sqrt(2.0));
}

// ============================================================
// Black-Scholes European call / put (optional sanity baseline)
// ============================================================
double bs_call(double S, double K, double T, double r, double sigma, double q = 0.0)
{
    if (T <= 0.0)
        return std::max(S - K, 0.0);
    if (sigma <= 0.0)
        return std::max(S * std::exp(-q * T) - K * std::exp(-r * T), 0.0);

    double sqrtT = std::sqrt(T);
    double d1 = (std::log(S / K) + (r - q + 0.5 * sigma * sigma) * T) / (sigma * sqrtT);
    double d2 = d1 - sigma * sqrtT;

    return S * std::exp(-q * T) * norm_cdf(d1) - K * std::exp(-r * T) * norm_cdf(d2);
}

double bs_put(double S, double K, double T, double r, double sigma, double q = 0.0)
{
    if (T <= 0.0)
        return std::max(K - S, 0.0);
    if (sigma <= 0.0)
        return std::max(K * std::exp(-r * T) - S * std::exp(-q * T), 0.0);

    double sqrtT = std::sqrt(T);
    double d1 = (std::log(S / K) + (r - q + 0.5 * sigma * sigma) * T) / (sigma * sqrtT);
    double d2 = d1 - sigma * sqrtT;

    return K * std::exp(-r * T) * norm_cdf(-d2) - S * std::exp(-q * T) * norm_cdf(-d1);
}

// ============================================================
// American FD pricer (Crank-Nicolson + PSOR)
// Solves Black-Scholes PDE on S-grid
//
// dV/dt + 0.5*sigma^2*S^2*V_SS + (r-q)*S*V_S - rV = 0
//
// Backward in time with early exercise:
//   V = max(continuation, intrinsic)
//
// This version uses:
// - uniform S-grid
// - Crank-Nicolson time stepping
// - PSOR for American constraint
// ============================================================
double american_fd_price(
    double S0,
    double K,
    double T,
    double r,
    double sigma,
    double q,
    bool is_call,
    int M = 400,            // space steps
    int N = 400,            // time steps
    double Smax_mult = 4.0, // Smax = max(S0, K) * Smax_mult
    double omega = 1.4,     // PSOR relaxation
    double tol = 1e-8,
    int max_iter = 10000)
{
    // ------------------------------
    // Basic validation
    // ------------------------------
    if (S0 <= 0.0 || K <= 0.0)
        throw std::runtime_error("S0 and K must be positive");
    if (T < 0.0)
        throw std::runtime_error("T must be non-negative");
    if (sigma < 0.0)
        throw std::runtime_error("sigma must be non-negative");
    if (M < 10 || N < 1)
        throw std::runtime_error("M and N too small");

    if (T == 0.0)
    {
        return is_call ? std::max(S0 - K, 0.0) : std::max(K - S0, 0.0);
    }

    // If sigma is ~0, American degenerates to discounted intrinsic/carry logic,
    // but for robustness just return immediate intrinsic as a safe fallback.
    if (sigma < 1e-12)
    {
        return is_call ? std::max(S0 - K, 0.0) : std::max(K - S0, 0.0);
    }

    // ------------------------------
    // Grid setup
    // ------------------------------
    double Smax = std::max(S0, K) * Smax_mult;
    if (Smax < 2.0 * K)
        Smax = 2.0 * K; // ensure enough upper room

    double dS = Smax / M;
    double dt = T / N;

    std::vector<double> Sgrid(M + 1);
    for (int i = 0; i <= M; ++i)
        Sgrid[i] = i * dS;

    // Value at current time layer and previous layer
    std::vector<double> V(M + 1), Vnew(M + 1), payoff(M + 1);

    // Terminal payoff at maturity
    for (int i = 0; i <= M; ++i)
    {
        payoff[i] = is_call ? std::max(Sgrid[i] - K, 0.0)
                            : std::max(K - Sgrid[i], 0.0);
        V[i] = payoff[i];
    }

    // Interior system size = M-1 nodes (i=1..M-1)
    int n_interior = M - 1;
    std::vector<double> lower(n_interior, 0.0); // sub-diagonal (A)
    std::vector<double> diag(n_interior, 0.0);  // diagonal (A)
    std::vector<double> upper(n_interior, 0.0); // super-diagonal (A)
    std::vector<double> rhs(n_interior, 0.0);

    // For matrix B (right-hand side)
    std::vector<double> lowerB(n_interior, 0.0);
    std::vector<double> diagB(n_interior, 0.0);
    std::vector<double> upperB(n_interior, 0.0);

    // ------------------------------
    // Backward time stepping
    // ------------------------------
    for (int n = N - 1; n >= 0; --n)
    {
        double t = n * dt;

        // Boundary conditions at time t
        double tau = T - t; // remaining time from current layer to maturity

        double left_bc, right_bc;

        if (is_call)
        {
            // At S=0, call = 0
            left_bc = 0.0;

            // At large S, call ~ S*e^{-q tau} - K*e^{-r tau}
            right_bc = Smax * std::exp(-q * tau) - K * std::exp(-r * tau);
            if (right_bc < Smax - K)
            {
                // not strictly necessary, but keep it sane
                right_bc = std::max(right_bc, Smax - K);
            }
        }
        else
        {
            // At S=0, put = K*e^{-r tau} (European asymptotic), but American put can be exercised immediately
            left_bc = K; // American boundary is stronger / safer

            // At large S, put ~ 0
            right_bc = 0.0;
        }

        Vnew[0] = std::max(left_bc, payoff[0]);
        Vnew[M] = std::max(right_bc, payoff[M]);

        // Build Crank-Nicolson matrices for interior nodes
        //
        // Indexing:
        // PDE operator at node i:
        // L V_i = alpha_i V_{i-1} + beta_i V_i + gamma_i V_{i+1}
        //
        // CN step:
        // (I - 0.5 dt L) V^n = (I + 0.5 dt L) V^{n+1}
        //
        for (int i = 1; i <= M - 1; ++i)
        {
            double ii = static_cast<double>(i);

            double alpha = 0.5 * sigma * sigma * ii * ii - 0.5 * (r - q) * ii;
            double beta = -sigma * sigma * ii * ii - r;
            double gamma = 0.5 * sigma * sigma * ii * ii + 0.5 * (r - q) * ii;

            int j = i - 1; // interior index 0..M-2

            // Left matrix A = I - 0.5 dt L
            lower[j] = -0.5 * dt * alpha;
            diag[j] = 1.0 - 0.5 * dt * beta;
            upper[j] = -0.5 * dt * gamma;

            // Right matrix B = I + 0.5 dt L
            lowerB[j] = 0.5 * dt * alpha;
            diagB[j] = 1.0 + 0.5 * dt * beta;
            upperB[j] = 0.5 * dt * gamma;
        }

        // Build RHS = B * V(old) + boundary contributions
        for (int i = 1; i <= M - 1; ++i)
        {
            int j = i - 1;

            rhs[j] = diagB[j] * V[i] + lowerB[j] * V[i - 1] + upperB[j] * V[i + 1];
        }

        // Move known boundary terms from A * Vnew = rhs
        //
        // First interior node i=1 has lower * Vnew[0]
        rhs[0] -= lower[0] * Vnew[0];

        // Last interior node i=M-1 has upper * Vnew[M]
        rhs[n_interior - 1] -= upper[n_interior - 1] * Vnew[M];

        // ------------------------------
        // PSOR solve with American constraint
        // Solve A x = rhs, with x >= payoff
        // ------------------------------

        // Initial guess: previous time layer interior values
        for (int i = 1; i <= M - 1; ++i)
            Vnew[i] = V[i];

        for (int iter = 0; iter < max_iter; ++iter)
        {
            double max_err = 0.0;

            for (int i = 1; i <= M - 1; ++i)
            {
                int j = i - 1;

                double sum = rhs[j];

                if (i > 1)
                    sum -= lower[j] * Vnew[i - 1];
                else
                    sum -= lower[j] * Vnew[0];

                if (i < M - 1)
                    sum -= upper[j] * Vnew[i + 1];
                else
                    sum -= upper[j] * Vnew[M];

                double old_val = Vnew[i];
                double gs = sum / diag[j];

                // SOR update
                double sor = old_val + omega * (gs - old_val);

                // American projection
                double projected = std::max(sor, payoff[i]);

                Vnew[i] = projected;

                double err = std::fabs(Vnew[i] - old_val);
                if (err > max_err)
                    max_err = err;
            }

            if (max_err < tol)
                break;

            if (iter == max_iter - 1)
            {
                // not fatal, but if you want stricter behavior, throw instead
                // throw std::runtime_error("PSOR did not converge");
            }
        }

        // Roll back one layer
        V.swap(Vnew);
    }

    // ------------------------------
    // Interpolate price at S0
    // ------------------------------
    if (S0 >= Smax)
    {
        return V[M];
    }

    int idx = static_cast<int>(S0 / dS);
    if (idx < 0)
        idx = 0;
    if (idx >= M)
        idx = M - 1;

    double S_left = Sgrid[idx];
    double S_right = Sgrid[idx + 1];

    if (std::fabs(S_right - S_left) < 1e-14)
        return V[idx];

    double w = (S0 - S_left) / (S_right - S_left);
    return (1.0 - w) * V[idx] + w * V[idx + 1];
}

// ============================================================
// Python wrappers
// ============================================================
double american_fd_call(
    double S0,
    double K,
    double T,
    double r,
    double sigma,
    double q = 0.0,
    int M = 400,
    int N = 400,
    double Smax_mult = 4.0,
    double omega = 1.4,
    double tol = 1e-8,
    int max_iter = 10000)
{
    return american_fd_price(S0, K, T, r, sigma, q, true, M, N, Smax_mult, omega, tol, max_iter);
}

double american_fd_put(
    double S0,
    double K,
    double T,
    double r,
    double sigma,
    double q = 0.0,
    int M = 400,
    int N = 400,
    double Smax_mult = 4.0,
    double omega = 1.4,
    double tol = 1e-8,
    int max_iter = 10000)
{
    return american_fd_price(S0, K, T, r, sigma, q, false, M, N, Smax_mult, omega, tol, max_iter);
}

// ============================================================
// PYBIND11 MODULE
// IMPORTANT: module name must match output filename stem
// e.g. if output is library.cpython-...so, module name = library
// ============================================================
PYBIND11_MODULE(USoptions_lib, m)
{
    m.doc() = "Option pricing library: BS + American FD (Crank-Nicolson + PSOR)";

    m.def("norm_cdf", &norm_cdf, "Standard normal CDF");

    m.def("bs_call", &bs_call,
          py::arg("S"),
          py::arg("K"),
          py::arg("T"),
          py::arg("r"),
          py::arg("sigma"),
          py::arg("q") = 0.0);

    m.def("bs_put", &bs_put,
          py::arg("S"),
          py::arg("K"),
          py::arg("T"),
          py::arg("r"),
          py::arg("sigma"),
          py::arg("q") = 0.0);

    m.def("american_fd_call", &american_fd_call,
          py::arg("S0"),
          py::arg("K"),
          py::arg("T"),
          py::arg("r"),
          py::arg("sigma"),
          py::arg("q") = 0.0,
          py::arg("M") = 400,
          py::arg("N") = 400,
          py::arg("Smax_mult") = 4.0,
          py::arg("omega") = 1.4,
          py::arg("tol") = 1e-8,
          py::arg("max_iter") = 10000);

    m.def("american_fd_put", &american_fd_put,
          py::arg("S0"),
          py::arg("K"),
          py::arg("T"),
          py::arg("r"),
          py::arg("sigma"),
          py::arg("q") = 0.0,
          py::arg("M") = 400,
          py::arg("N") = 400,
          py::arg("Smax_mult") = 4.0,
          py::arg("omega") = 1.4,
          py::arg("tol") = 1e-8,
          py::arg("max_iter") = 10000);
}