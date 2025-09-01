#pragma once
/*
Anytime-valid Empirical–Bernstein Confidence Sequence for a bounded mean in [0,1].

We estimate the **settlement probability** p = E[X], where X∈{0,1} indicates a run settled.
At sample size n (n>=2), with empirical mean mu_hat and empirical variance vhat = mu_hat*(1-mu_hat),
the two-sided half-width is

  w_n = sqrt( 2 * vhat * L_n / n ) + 3 * L_n / (n - 1),

where L_n = log(6 / delta_n), and delta_n = alpha * 6 / (pi^2 * n^2)
allocates risk over times so that sum_n delta_n <= alpha (anytime-valid).
We stop when 2 * w_n <= eps.

References:
- Maurer & Pontil (2009), "Empirical Bernstein Bounds and Sample Variance Penalization".
- Howard et al. (2021), "Time-uniform, nonparametric, nonasymptotic...": we achieve
  time-uniformity via the 1/n^2 union schedule.
*/

#include <cstddef>
#include <cmath>
#include <limits>

namespace ebcs {

inline double pi() { return 3.141592653589793238462643383279502884; }

inline double log_pos(double x) {
    return (x > 0.0) ? std::log(x) : std::numeric_limits<double>::infinity();
}

struct EBState {
    std::size_t n{0};
    std::size_t sum{0}; // sum X_i for Bernoulli -> mu_hat = sum/n
    double mu_hat{0.0};
    double vhat{0.0};
    double w{std::numeric_limits<double>::infinity()};
    double two_w{std::numeric_limits<double>::infinity()};
};

inline EBState update_and_bound(std::size_t n, std::size_t sum, double alpha) {
    EBState s{};
    s.n = n; s.sum = sum;
    if (n == 0) return s;
    s.mu_hat = static_cast<double>(sum) / static_cast<double>(n);
    s.vhat   = s.mu_hat * (1.0 - s.mu_hat);

    if (n < 2 || !(alpha > 0.0) || !(alpha < 1.0)) {
        return s; // w stays inf
    }
    const double nd = static_cast<double>(n);
    const double delta_n = alpha * 6.0 / (pi()*pi() * nd * nd); // time allocation
    const double L = log_pos(6.0 / delta_n);

    const double term1 = std::sqrt( (2.0 * s.vhat * L) / nd );
    const double term2 = (3.0 * L) / (nd - 1.0);
    s.w = term1 + term2;
    s.two_w = 2.0 * s.w;
    return s;
}

inline bool should_stop(const EBState& s, double eps) {
    return (s.two_w <= eps);
}

} // namespace ebcs
