#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace py = pybind11;

// =========================
// Normal PDF and CDF
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

double forward_price(double S, double r, double T)
{
    if (S <= 0 || T <= 0)
        throw std::invalid_argument("S and T must be positive.");
    return S * std::exp(r * T);
}

// =========================
// Black-Scholes call price
// =========================
double bs_call(double S, double K, double T, double r, double sigma)
{
    if (S <= 0 || K <= 0 || T <= 0 || sigma <= 0)
        throw std::invalid_argument("Inputs must be positive.");

    double sqrtT = std::sqrt(T);
    double d1 = (std::log(S / K) + (r + 0.5 * sigma * sigma) * T) / (sigma * sqrtT);
    double d2 = d1 - sigma * sqrtT;

    return S * norm_cdf(d1) - K * std::exp(-r * T) * norm_cdf(d2);
}

// =========================
// Black-Scholes put price
// =========================
double bs_put(double S, double K, double T, double r, double sigma)
{
    if (S <= 0 || K <= 0 || T <= 0 || sigma <= 0)
        throw std::invalid_argument("Inputs must be positive.");

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
    if (S <= 0 || K <= 0 || T <= 0 || sigma <= 0)
        throw std::invalid_argument("Inputs must be positive.");

    double sqrtT = std::sqrt(T);
    double d1 = (std::log(S / K) + (r + 0.5 * sigma * sigma) * T) / (sigma * sqrtT);

    return norm_cdf(d1);
}

double bs_delta_put(double S, double K, double T, double r, double sigma)
{
    if (S <= 0 || K <= 0 || T <= 0 || sigma <= 0)
        throw std::invalid_argument("Inputs must be positive.");

    double sqrtT = std::sqrt(T);
    double d1 = (std::log(S / K) + (r + 0.5 * sigma * sigma) * T) / (sigma * sqrtT);

    return norm_cdf(d1) - 1.0;
}

// =========================
// Theta (per day)
// =========================
double bs_theta_call(double S, double K, double T, double r, double sigma)
{
    if (S <= 0 || K <= 0 || T <= 0 || sigma <= 0)
        throw std::invalid_argument("Inputs must be positive.");

    double sqrtT = std::sqrt(T);
    double d1 = (std::log(S / K) + (r + 0.5 * sigma * sigma) * T) / (sigma * sqrtT);
    double d2 = d1 - sigma * sqrtT;

    double term1 = -(S * norm_pdf(d1) * sigma) / (2.0 * sqrtT);
    double term2 = -r * K * std::exp(-r * T) * norm_cdf(d2);

    return (term1 + term2) / 365.0;
}

double bs_theta_put(double S, double K, double T, double r, double sigma)
{
    if (S <= 0 || K <= 0 || T <= 0 || sigma <= 0)
        throw std::invalid_argument("Inputs must be positive.");

    double sqrtT = std::sqrt(T);
    double d1 = (std::log(S / K) + (r + 0.5 * sigma * sigma) * T) / (sigma * sqrtT);
    double d2 = d1 - sigma * sqrtT;

    double term1 = -(S * norm_pdf(d1) * sigma) / (2.0 * sqrtT);
    double term2 = r * K * std::exp(-r * T) * norm_cdf(-d2);

    return (term1 + term2) / 365.0;
}

// =========================
// Vega (per 1 vol point = 1%)
// =========================
double bs_vega(double S, double K, double T, double r, double sigma)
{
    if (S <= 0 || K <= 0 || T <= 0 || sigma <= 0)
        throw std::invalid_argument("Inputs must be positive.");

    double sqrtT = std::sqrt(T);
    double d1 = (std::log(S / K) + (r + 0.5 * sigma * sigma) * T) / (sigma * sqrtT);

    return S * norm_pdf(d1) * sqrtT / 100.0;
}

// =========================
// SVI total variance
// =========================

// =========================
// SVI implied vol from strike
// =========================

double svi_total_variance_scalar(double k, double a, double b, double rho, double m, double sigma)
{
    double diff = k - m;
    return a + b * (rho * diff + std::sqrt(diff * diff + sigma * sigma));
}

// =========================
// Vectorized SVI for NumPy
// =========================
py::array_t<double> svi_total_variance(py::array_t<double> k_array,
                                       double a, double b, double rho, double m, double sigma)
{
    auto buf = k_array.request();
    if (buf.ndim != 1)
        throw std::runtime_error("Input must be a 1D array");

    size_t n = buf.shape[0];
    double *ptr = static_cast<double *>(buf.ptr);

    py::array_t<double> result(n);
    auto res_buf = result.request();
    double *res_ptr = static_cast<double *>(res_buf.ptr);

    for (size_t i = 0; i < n; ++i)
    {
        double diff = ptr[i] - m;
        res_ptr[i] = a + b * (rho * diff + std::sqrt(diff * diff + sigma * sigma));
    }
    return result;
}

double svi_vol(double K, double S, double T, double r,
               double a, double b, double rho, double m, double sigma)
{
    if (K <= 0 || S <= 0 || T <= 0)
        throw std::invalid_argument("K, S, T must be positive.");

    double F = S * std::exp(r * T);
    double k = std::log(K / F);
    double w = svi_total_variance_scalar(k, a, b, rho, m, sigma);

    if (w <= 0.0)
        throw std::runtime_error("SVI total variance is non-positive.");

    return std::sqrt(w / T);
}

// =========================
// Module
// =========================
PYBIND11_MODULE(library, m)
{
    m.doc() = "Quant library with Black-Scholes and SVI functions";

    m.def("norm_pdf", &norm_pdf, "Standard normal PDF");
    m.def("norm_cdf", &norm_cdf, "Standard normal CDF");

    m.def("bs_call", &bs_call, "Black-Scholes call price");
    m.def("bs_put", &bs_put, "Black-Scholes put price");

    m.def("bs_delta_call", &bs_delta_call, "Black-Scholes call delta");
    m.def("bs_delta_put", &bs_delta_put, "Black-Scholes put delta");

    m.def("bs_theta_call", &bs_theta_call, "Black-Scholes call theta (per day)");
    m.def("bs_theta_put", &bs_theta_put, "Black-Scholes put theta (per day)");

    m.def("bs_vega", &bs_vega, "Black-Scholes vega (per 1% vol move)");

    m.def("svi_total_variance", &svi_total_variance, "SVI total variance");
    m.def("svi_vol", &svi_vol, "SVI implied vol from strike");

    m.def("svi_total_variance_scalar", &svi_total_variance_scalar, "SVI total variance for scalar k");
}
