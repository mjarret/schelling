#pragma once
#include <cstdint>
#include <cmath>
#include <limits>

namespace cs {

// Anytime-valid Hoeffding half-width for a bounded-mean vector curve on K points in [0,1].
inline double halfwidth_anytime_hoeffding(uint64_t n, uint64_t K, double alpha, double range = 1.0) {
    if (n == 0) return std::numeric_limits<double>::infinity();
    constexpr double PI = 3.14159265358979323846264338327950288;
    const double nn = static_cast<double>(n);
    const double logterm = std::log( (PI*PI * static_cast<double>(K) * nn * nn) / (3.0 * alpha) );
    const double hw = (range / std::sqrt(2.0 * nn)) * std::sqrt(std::max(0.0, logterm));
    return hw;
}

// Stop when 2 * w_n <= eps (time-uniform guarantee).
inline bool should_stop(uint64_t n, uint64_t K, double alpha, double eps, double range = 1.0) {
    if (n == 0) return false;
    const double w = halfwidth_anytime_hoeffding(n, K, alpha, range);
    return (2.0 * w <= eps);
}

} // namespace cs
