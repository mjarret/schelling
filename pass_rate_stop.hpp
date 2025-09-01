// pass_rate_stop.hpp — Blockwise pass‑rate stopping with Clopper–Pearson and α‑spending.
#pragma once
#include <vector>
#include <mutex>
#include <cmath>
#include <algorithm>

namespace passrate {

inline double binom_sf_ge(int n, int s, double p) {
    if (s <= 0) return 1.0;
    if (s > n)  return 0.0;
    if (p <= 0.0) return (s == 0 ? 1.0 : 0.0);
    if (p >= 1.0) return 1.0;
    auto log_pmf = [&](int k)->double {
        return std::lgamma(double(n+1)) - std::lgamma(double(k+1)) - std::lgamma(double(n-k+1))
             + k*std::log(p) + (n-k)*std::log(1.0 - p);
    };
    double term = std::exp(log_pmf(s)), sum = term;
    for (int k = s; k < n; ++k) {
        term *= ((double)(n - k) / (double)(k + 1)) * (p / (1.0 - p));
        sum  += term;
        if (term < 1e-16 * sum) break;
    }
    if (sum < 0.0) sum = 0.0;
    if (sum > 1.0) sum = 1.0;
    return sum;
}

inline double clopper_pearson_lower(int s, int n, double alpha) {
    if (n <= 0) return 0.0;
    if (s <= 0) return 0.0;
    if (s >  n) return 0.0;
    if (alpha <= 0.0) return 1.0;
    if (alpha >= 1.0) return 0.0;
    double lo = 0.0, hi = 1.0;
    for (int it = 0; it < 60; ++it) {
        double mid = 0.5 * (lo + hi);
        double sf = binom_sf_ge(n, s, mid);
        if (sf >= alpha) hi = mid; else lo = mid;
    }
    return std::max(0.0, std::min(1.0, hi));
}

inline double alpha_block(double alpha_family, uint32_t b) {
    static const double c = 6.0 / (std::acos(-1.0) * std::acos(-1.0)); // 6/pi^2
    return alpha_family * c / double(b) / double(b);
}

class PassRateStopper {
public:
    PassRateStopper(double q_target, double alpha_total, size_t F, uint32_t window)
    : q_target_(q_target),
      alpha_total_(alpha_total),
      F_(F),
      Y_(window),
      alpha_family_((F > 0 ? alpha_total / double(F) : alpha_total)),
      seen_(F, 0),
      succ_(F, 0),
      block_(F, 1),
      last_L_(F, 0.0),
      done_(F, false),
      mx_(F) {}

    bool record(size_t f, bool pass, double* L_out=nullptr, uint32_t* b_out=nullptr,
                uint32_t* s_in_blk=nullptr, uint32_t* n_in_blk=nullptr) {
        std::lock_guard<std::mutex> lk(mx_[f]);
        if (done_[f]) {
            if (L_out) *L_out = last_L_[f];
            if (b_out) *b_out = block_[f];
            if (s_in_blk) *s_in_blk = succ_[f];
            if (n_in_blk) *n_in_blk = seen_[f];
            return false;
        }
        seen_[f] += 1;
        if (pass) succ_[f] += 1;

        if (seen_[f] >= Y_) {
            uint32_t b = block_[f];
            double alpha_b = alpha_block(alpha_family_, b);
            double L = clopper_pearson_lower((int)succ_[f], (int)seen_[f], alpha_b);
            last_L_[f] = L;
            if (L >= q_target_) {
                done_[f] = true;
                if (L_out) *L_out = L;
                if (b_out) *b_out = b;
                if (s_in_blk) *s_in_blk = succ_[f];
                if (n_in_blk) *n_in_blk = seen_[f];
                return true;
            }
            block_[f] += 1;
            seen_[f] = 0;
            succ_[f] = 0;
        }

        if (L_out) *L_out = last_L_[f];
        if (b_out) *b_out = block_[f];
        if (s_in_blk) *s_in_blk = succ_[f];
        if (n_in_blk) *n_in_blk = seen_[f];
        return false;
    }

    bool family_done(size_t f) const { return done_[f]; }
    double min_last_lower() const {
        double m = 1.0; bool any=false;
        for (size_t f=0; f<F_; ++f) { m = std::min(m, last_L_[f]); any |= (last_L_[f] > 0.0); }
        return any ? m : 0.0;
    }

private:
    double q_target_;
    double alpha_total_;
    size_t F_;
    uint32_t Y_;
    double alpha_family_;

    std::vector<uint32_t> seen_, succ_, block_;
    std::vector<double>   last_L_;
    std::vector<bool>     done_;
    std::vector<std::mutex> mx_;
};

} // namespace passrate
