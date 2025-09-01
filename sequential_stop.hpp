// sequential_stop.hpp
#pragma once
#include <cmath>
#include <cstdint>
#include <vector>
#include <atomic>
#include <algorithm>
#include <limits>
#include "cost_aggregator.hpp"

namespace seqstop {

// Simple summable alpha-spending schedule over time n: α_n = α_cap * c / n^p
// Here we use p=2 with c = 1 / ζ(2) = 6 / π^2 so that sum α_n = α_cap.
// Two-sided control is handled inside radius formulas (log factors).
struct AlphaSchedule {
    static inline double inv_zeta2() {
        // 6/pi^2
        static const double v = 6.0 / (std::acos(-1.0) * std::acos(-1.0));
        return v;
    }
    static inline double alpha_n(uint64_t n, double alpha_cap) {
        if (n == 0) n = 1;
        return alpha_cap * inv_zeta2() / (double(n) * double(n));
    }
};

// Fixed-time Hoeffding radius for bounded [0,1] variables, but fed α_n (per-time budget).
// Two-sided: use log(2/α_n).
inline double radius_hoeffding(uint64_t n, double alpha_n) {
    if (n == 0) return std::numeric_limits<double>::infinity();
    double ln = std::log(2.0 / std::max(alpha_n, 1e-300));
    return std::sqrt(ln / (2.0 * double(n)));
}

// Fixed-time Empirical-Bernstein radius (Maurer-Pontil style), fed α_n.
// Two-sided via log(3/α_n). Needs mean and second moment (in [0,1]).
inline double radius_empbern(uint64_t n, double mean, double second_moment, double alpha_n) {
    if (n <= 1) return std::numeric_limits<double>::infinity();
    double var_hat = std::max(0.0, second_moment - mean * mean);
    double ln = std::log(3.0 / std::max(alpha_n, 1e-300));
    double term1 = std::sqrt(std::max(0.0, 2.0 * var_hat * ln / double(n)));
    double term2 = 3.0 * ln / double(n);
    return term1 + term2;
}

// A sequential stopper for the COST curve (mean unhappy fraction).
// It uses α-spending across time and splits α across families and checkpoints.
// Guarantee: with prob ≥ 1-α_total, for all n and all checkpoints in all families,
// |mean_n - μ| ≤ ε at the time we stop.
class SequentialCostStopper {
public:
    // eps: target uniform half-width
    // alpha_total: global failure budget for all families and checkpoints
    // F: number of families
    // m_per_f: # checkpoints per family
    SequentialCostStopper(double eps, double alpha_total, size_t F, uint64_t m_per_f, bool use_empbern)
    : eps_(eps), alpha_total_(alpha_total), F_(F), m_(m_per_f), use_empbern_(use_empbern),
      done_(F), min_n_seen_(F, 0)
    {
        for (auto& d : done_) d.store(false, std::memory_order_relaxed);
        // Split α across family/checkpoint pairs (Bonferroni across class).
        alpha_per_cp_ = alpha_total_ / std::max<double>(1.0, double(F_) * double(m_));
        // Ensure positive
        if (alpha_per_cp_ <= 0.0) alpha_per_cp_ = 1e-12;
    }

    bool family_done(size_t f) const {
        return done_[f].load(std::memory_order_relaxed);
    }

    // Check family f given its CostAggregator. If all checkpoints meet eps, mark done.
    // Returns true if newly done or already done. Also returns min count across checkpoints.
    bool check_and_mark(size_t f, const CostAggregator& aggr, uint64_t& n_min_out) {
        if (family_done(f)) {
            n_min_out = min_n_seen_[f];
            return true;
        }
        const size_t m = aggr.m;
        double worst_r = 0.0;
        uint64_t n_min = std::numeric_limits<uint64_t>::max();

        for (size_t k = 0; k < m; ++k) {
            uint64_t n = aggr.count_runs[k].load(std::memory_order_relaxed);
            n_min = std::min<uint64_t>(n_min, n);
            if (n == 0) { worst_r = std::numeric_limits<double>::infinity(); continue; }

            double alpha_n = AlphaSchedule::alpha_n(n, alpha_per_cp_);
            // Mean in [0,1]
            double mean = 0.0, m2 = 0.0;
            {
                uint64_t sum = aggr.sum_frac_scaled[k].load(std::memory_order_relaxed);
                uint64_t ss  = aggr.sum_frac_sq_scaled[k].load(std::memory_order_relaxed);
                mean = double(sum) / double(n) / double(CostAggregator::SCALE);
                m2   = double(ss)  / double(n) / double(CostAggregator::SCALE) / double(CostAggregator::SCALE);
                // clamp to [0,1] in case of numeric drift
                mean = std::clamp(mean, 0.0, 1.0);
                m2   = std::clamp(m2,   0.0, 1.0);
            }

            double rH = radius_hoeffding(n, alpha_n);
            double rE = use_empbern_ ? radius_empbern(n, mean, m2, alpha_n)
                                     : std::numeric_limits<double>::infinity();
            double r = std::min(rH, rE);
            if (!std::isfinite(r)) r = rH;
            worst_r = std::max(worst_r, r);
            if (worst_r > eps_) {
                min_n_seen_[f] = (n_min == std::numeric_limits<uint64_t>::max() ? 0ull : n_min);
                return false;
            }
        }
        // If we reached here, all checkpoints satisfy r <= eps
        done_[f].store(true, std::memory_order_relaxed);
        min_n_seen_[f] = (n_min == std::numeric_limits<uint64_t>::max() ? 0ull : n_min);
        return true;
    }

    // Compute current worst half-width across all not-yet-done families (for display).
    double worst_radius_overall(const std::vector<std::unique_ptr<CostAggregator>>& aggrs) const {
        double worst = 0.0;
        for (size_t f=0; f<F_; ++f) {
            if (family_done(f)) continue;
            const auto& aggr = *aggrs[f];
            for (size_t k=0; k<aggr.m; ++k) {
                uint64_t n = aggr.count_runs[k].load(std::memory_order_relaxed);
                if (n == 0) return std::numeric_limits<double>::infinity();
                double alpha_n = AlphaSchedule::alpha_n(n, alpha_per_cp_);
                uint64_t sum = aggr.sum_frac_scaled[k].load(std::memory_order_relaxed);
                uint64_t ss  = aggr.sum_frac_sq_scaled[k].load(std::memory_order_relaxed);
                double mean = double(sum) / double(n) / double(CostAggregator::SCALE);
                double m2   = double(ss)  / double(n) / double(CostAggregator::SCALE) / double(CostAggregator::SCALE);
                mean = std::clamp(mean, 0.0, 1.0); m2 = std::clamp(m2, 0.0, 1.0);
                double rH = radius_hoeffding(n, alpha_n);
                double rE = use_empbern_ ? radius_empbern(n, mean, m2, alpha_n)
                                         : std::numeric_limits<double>::infinity();
                double r = std::min(rH, rE);
                if (!std::isfinite(r)) r = rH;
                if (r > worst) worst = r;
            }
        }
        return worst;
    }

private:
    double eps_, alpha_total_;
    size_t F_;
    uint64_t m_;
    bool use_empbern_;
    double alpha_per_cp_;
    std::vector<std::atomic<bool>> done_;
    std::vector<uint64_t> min_n_seen_;
};

} // namespace seqstop
