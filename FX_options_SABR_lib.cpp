#include <pybind11/pybind11.h>
#include <cmath>
#include <stdexcept>

namespace py = pybind11;

// =========================
// Normal PDF / CDF
// =========================
double norm_pdf(double x)
{
    static const double INV_SQRT_2PI = 0.3989422804014327;
    return INV_SQRT_2PI * std::exp(-0.5 * x * x);
}

double norm_cdf(double x)
{
    return 0.5 * std::erfc(-x / std::sqrt(2.0));
}

// =========================
// Black-Scholes
// =========================
double bs_call(double S, double K, double T, double r, double sigma)
{
    double sqrtT = std::sqrt(T);
    double d1 = (std::log(S / K) + (r + 0.5 * sigma * sigma) * T) / (sigma * sqrtT);
    double d2 = d1 - sigma * sqrtT;

    return S * norm_cdf(d1) - K * std::exp(-r * T) * norm_cdf(d2);
}

double bs_put(double S, double K, double T, double r, double sigma)
{
    double sqrtT = std::sqrt(T);
    double d1 = (std::log(S / K) + (r + 0.5 * sigma * sigma) * T) / (sigma * sqrtT);
    double d2 = d1 - sigma * sqrtT;

    return K * std::exp(-r * T) * norm_cdf(-d2) - S * norm_cdf(-d1);
}

// =========================
// Delta
// =========================
double bs_delta_call(double S, double K, double T, double r, double sigma)
{
    double sqrtT = std::sqrt(T);
    double d1 = (std::log(S / K) + (r + 0.5 * sigma * sigma) * T) / (sigma * sqrtT);
    return norm_cdf(d1);
}

double bs_delta_put(double S, double K, double T, double r, double sigma)
{
    return bs_delta_call(S, K, T, r, sigma) - 1.0;
}

double bs_vega(double S, double K, double T, double r, double sigma)
{
    double sqrtT = std::sqrt(T);
    double d1 = (std::log(S / K) + (r + 0.5 * sigma * sigma) * T) / (sigma * sqrtT);
    return S * norm_pdf(d1) * sqrtT;
}

// =========================
// SABR VOL (Hagan)
// =========================
double sabr_vol(double F, double K, double T,
                double alpha, double beta,
                double rho, double nu)
{
    double logFK = std::log(F / K);
    double FK_beta = std::pow(F * K, (1.0 - beta) / 2.0);

    // ATM
    if (std::abs(logFK) < 1e-07)
    {
        double term1 = alpha / std::pow(F, 1.0 - beta);

        double term2 =
            ((1 - beta) * (1 - beta) / 24.0 * alpha * alpha / std::pow(F, 2 - 2 * beta) + 0.25 * rho * beta * nu * alpha / std::pow(F, 1 - beta) + (2 - 3 * rho * rho) / 24.0 * nu * nu) * T;

        return term1 * (1.0 + term2);
    }

    double z = (nu / alpha) * FK_beta * logFK;
    double xz = std::log((std::sqrt(1 - 2 * rho * z + z * z) + z - rho) / (1 - rho));

    double denom = FK_beta *
                   (1 + (1 - beta) * (1 - beta) / 24.0 * logFK * logFK + std::pow(1 - beta, 4) / 1920.0 * std::pow(logFK, 4));

    double vol = (alpha / denom) * (z / xz);

    double correction =
        ((1 - beta) * (1 - beta) / 24.0 * alpha * alpha / std::pow(F * K, 1 - beta) + 0.25 * rho * beta * nu * alpha / FK_beta + (2 - 3 * rho * rho) / 24.0 * nu * nu) * T;

    return vol * (1.0 + correction);
}

// =========================
// Strike from delta
// =========================
double strike_from_delta(double S, double T, double r,
                         double sigma, double target_delta,
                         bool is_call)
{
    double K = S;

    for (int i = 0; i < 10; ++i)
    {
        double delta = is_call ? bs_delta_call(S, K, T, r, sigma) : bs_delta_put(S, K, T, r, sigma);

        double vega = bs_vega(S, K, T, r, sigma);

        double diff = delta - target_delta;
        if (std::abs(diff) < 1e-8)
            break;

        // dDelta/dK approx
        double dDelta_dK = -vega / (S * sigma);

        K -= diff / dDelta_dK;
    }

    return K;
}

// =========================
// Solve rho, nu (2D Newton)
// =========================
void solve_rho_nu(double F, double T,
                  double Kc, double Kp,
                  double vol_c, double vol_p,
                  double alpha, double beta,
                  double &rho, double &nu)
{
    rho = 0.0;
    nu = 0.5;

    for (int i = 0; i < 10; ++i)
    {
        double vc = sabr_vol(F, Kc, T, alpha, beta, rho, nu);
        double vp = sabr_vol(F, Kp, T, alpha, beta, rho, nu);

        double f1 = vc - vol_c;
        double f2 = vp - vol_p;

        if (std::abs(f1) + std::abs(f2) < 1e-8)
            break;

        double eps = 1e-5;

        double vc_r = sabr_vol(F, Kc, T, alpha, beta, rho + eps, nu);
        double vp_r = sabr_vol(F, Kp, T, alpha, beta, rho + eps, nu);

        double vc_n = sabr_vol(F, Kc, T, alpha, beta, rho, nu + eps);
        double vp_n = sabr_vol(F, Kp, T, alpha, beta, rho, nu + eps);

        double J11 = (vc_r - vc) / eps;
        double J21 = (vp_r - vp) / eps;
        double J12 = (vc_n - vc) / eps;
        double J22 = (vp_n - vp) / eps;

        double det = J11 * J22 - J12 * J21;

        double drho = (-f1 * J22 + f2 * J12) / det;
        double dnu = (-J11 * f2 + J21 * f1) / det;

        rho += drho;
        nu += dnu;

        rho = std::max(-0.999, std::min(0.999, rho));
        nu = std::max(1e-4, nu);
    }
}

// =========================
// FULL SABR CALIBRATION
// =========================
void sabr_calibrate(
    double S, double r, double T,
    double sigma_atm,
    double RR, double BF,
    double beta,
    double &alpha, double &rho, double &nu)
{
    double F = S * std::exp(r * T);

    // Convert quotes
    double vol_c = sigma_atm + BF + 0.5 * RR;
    double vol_p = sigma_atm + BF - 0.5 * RR;

    // Alpha initial guess
    alpha = sigma_atm * std::pow(F, 1.0 - beta);

    // Strikes
    double Kc = strike_from_delta(S, T, r, vol_c, 0.25, true);
    double Kp = strike_from_delta(S, T, r, vol_p, -0.25, false);

    // Solve rho, nu
    solve_rho_nu(F, T, Kc, Kp, vol_c, vol_p, alpha, beta, rho, nu);
}

// =========================
// PYBIND
// =========================
PYBIND11_MODULE(FX_options_SABR_lib, m)
{
    m.def("sabr_vol", &sabr_vol);

    m.def("sabr_calibrate",
          [](double S, double r, double T,
             double sigma_atm, double RR, double BF, double beta)
          {
              double alpha, rho, nu;
              sabr_calibrate(S, r, T, sigma_atm, RR, BF, beta,
                             alpha, rho, nu);
              return py::make_tuple(alpha, rho, nu);
          });
}