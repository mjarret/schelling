// stopping_bounds.hpp
#pragma once
#include <cstdint>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>

namespace stopping {

// What we want to control
enum class Target { COST, SURVIVAL };

// Strategy plan result
struct Plan {
    std::string name;
    // Per-family required runs (conservative): we return a single number to hit for every family.
    uint64_t n_required{0};
};

// Abstract bound (now with invertible helpers for reporting)
class IBoundStrategy {
public:
    virtual ~IBoundStrategy() = default;

    // Compute a plan (sample size) before running
    virtual Plan make_plan(double eps, double alpha_total,
                           const std::vector<uint64_t>& m_per_family,
                           uint64_t horizon,
                           size_t n_families,
                           double lipschitz_L = 0.0) const = 0;

    // --- Live reporting helpers ---
    // Given total alpha, current n for a *single* family, and the family-specific m/horizon/L,
    // return the uniform epsilon guarantee for that family. The strategy itself handles
    // the family split of alpha (i.e., α_f = α_total / n_families when appropriate).
    virtual double epsilon_given_n(double alpha_total,        // total across all families
                                   uint64_t n_family,         // current runs contributed by this family
                                   uint64_t m_family,         // e.g., #checkpoints (COST); 1 for SURVIVAL
                                   uint64_t horizon,
                                   size_t n_families,
                                   double lipschitz_L = 0.0) const = 0;

    // Given target epsilon, current n for a single family, return the *total* alpha bound
    // (union over families handled internally).
    virtual double alpha_total_given_n_eps(double eps_target,
                                           uint64_t n_family,
                                           uint64_t m_family,
                                           uint64_t horizon,
                                           size_t n_families,
                                           double lipschitz_L = 0.0) const = 0;
};

inline double _log_safe(double x) {
    if (x <= 0) x = 1e-300;
    return std::log(x);
}

inline uint64_t _max1(uint64_t x) { return x ? x : 1; }

// ---------- DKW for SURVIVAL (uniform over all T) ----------
class DKWBound final : public IBoundStrategy {
public:
    Plan make_plan(double eps, double alpha_total,
                   const std::vector<uint64_t>& /*m_per_family*/,
                   uint64_t /*horizon*/,
                   size_t n_families,
                   double /*lipschitz_L*/ = 0.0) const override {
        Plan p; p.name = "DKW (survival, uniform in T)";
        if (eps <= 0) { p.n_required = 0; return p; }
        if (n_families == 0) n_families = 1;
        double alpha_f = alpha_total / double(n_families);
        double req = _log_safe(2.0/alpha_f) / (2.0 * eps * eps);
        p.n_required = (req < 1.0 ? 1 : (uint64_t)std::ceil(req));
        return p;
    }

    double epsilon_given_n(double alpha_total, uint64_t n_family, uint64_t /*m_family*/,
                           uint64_t /*horizon*/, size_t n_families, double /*L*/ = 0.0) const override {
        n_family = _max1(n_family);
        if (n_families == 0) n_families = 1;
        double alpha_f = alpha_total / double(n_families);
        return std::sqrt(_log_safe(2.0/alpha_f) / (2.0 * double(n_family)));
    }

    double alpha_total_given_n_eps(double eps_target, uint64_t n_family, uint64_t /*m_family*/,
                                   uint64_t /*horizon*/, size_t n_families, double /*L*/ = 0.0) const override {
        n_family = _max1(n_family);
        if (n_families == 0) n_families = 1;
        double alpha_f = 2.0 * std::exp(-2.0 * double(n_family) * eps_target * eps_target);
        double alpha_tot = alpha_f * double(n_families);
        if (alpha_tot > 1.0) alpha_tot = 1.0;
        return alpha_tot;
    }
};

// ---------- Finite-class "Rademacher/Massart" for COST ----------
class RademacherFinite final : public IBoundStrategy {
public:
    Plan make_plan(double eps, double alpha_total,
                   const std::vector<uint64_t>& m_per_family,
                   uint64_t /*horizon*/,
                   size_t n_families,
                   double /*lipschitz_L*/ = 0.0) const override {
        Plan p; p.name = "Finite-class Rademacher (COST)";
        if (eps <= 0) { p.n_required = 0; return p; }
        if (n_families == 0) n_families = 1;
        double alpha_f = alpha_total / double(n_families);

        uint64_t nreq_max = 1;
        for (uint64_t m : m_per_family) {
            m = _max1(m);
            // Using: sup |μ̂−μ| ≤ sqrt((2 ln m + ln(2/α_f)) / (2 n))
            double req = (2.0*_log_safe(double(m)) + _log_safe(2.0/alpha_f)) / (2.0 * eps * eps);
            if (req < 1.0) req = 1.0;
            nreq_max = std::max<uint64_t>(nreq_max, (uint64_t)std::ceil(req));
        }
        p.n_required = nreq_max;
        return p;
    }

    double epsilon_given_n(double alpha_total, uint64_t n_family, uint64_t m_family,
                           uint64_t /*horizon*/, size_t n_families, double /*L*/ = 0.0) const override {
        n_family = _max1(n_family);
        m_family = _max1(m_family);
        if (n_families == 0) n_families = 1;
        double alpha_f = alpha_total / double(n_families);
        double num = 2.0*_log_safe(double(m_family)) + _log_safe(2.0/alpha_f);
        return std::sqrt( num / (2.0 * double(n_family)) );
    }

    double alpha_total_given_n_eps(double eps_target, uint64_t n_family, uint64_t m_family,
                                   uint64_t /*horizon*/, size_t n_families, double /*L*/ = 0.0) const override {
        n_family = _max1(n_family);
        m_family = _max1(m_family);
        // Rearranged: ln(2/α_f) = 2 n ε^2 − 2 ln m  => α_f = 2 m^2 e^{−2 n ε^2}
        double alpha_f = 2.0 * std::exp( 2.0*_log_safe(double(m_family)) - 2.0*double(n_family)*eps_target*eps_target );
        double alpha_tot = alpha_f * double(std::max<size_t>(1, n_families));
        if (alpha_tot > 1.0) alpha_tot = 1.0;
        return alpha_tot;
    }
};

// ---------- Lipschitz + finite-class ----------
class LipschitzRademacher final : public IBoundStrategy {
public:
    Plan make_plan(double eps, double alpha_total,
                   const std::vector<uint64_t>& /*m_per_family*/,
                   uint64_t horizon,
                   size_t n_families,
                   double L) const override {
        Plan p; p.name = "Lipschitz + Rademacher (COST)";
        if (eps <= 0) { p.n_required = 0; return p; }
        if (L <= 0.0) { // fallback to trivial finite-class with m=1
            RademacherFinite base;
            std::vector<uint64_t> m1(1, 1);
            return base.make_plan(eps, alpha_total, m1, horizon, n_families, 0.0);
        }
        if (n_families == 0) n_families = 1;
        double alpha_f = alpha_total / double(n_families);

        // Fixed-point solve for epsilon (inside make_plan, invert to n):
        // Use m_eff ≈ 1 + horizon * L / eps
        // => need n s.t. eps = sqrt((2 ln m_eff + ln(2/α_f)) / (2 n))
        // Rearrange to n ≥ (2 ln m_eff + ln(2/α_f)) / (2 ε^2), with m_eff = 1 + hL/ε (depends on ε).
        // We conservatively bound m_eff by using ε_target in denominator:
        uint64_t m_eff = 1 + (uint64_t)std::ceil( (double(horizon) * L) / std::max(eps, 1e-12) );
        double req = (2.0*_log_safe(double(m_eff)) + _log_safe(2.0/alpha_f)) / (2.0 * eps * eps);
        if (req < 1.0) req = 1.0;
        p.n_required = (uint64_t)std::ceil(req);
        return p;
    }

    double epsilon_given_n(double alpha_total, uint64_t n_family, uint64_t /*m_family*/,
                           uint64_t horizon, size_t n_families, double L) const override {
        n_family = _max1(n_family);
        if (n_families == 0) n_families = 1;
        double alpha_f = alpha_total / double(n_families);
        if (L <= 0.0) {
            // fallback to RademacherFinite with m=1
            double num = _log_safe(2.0/alpha_f);
            return std::sqrt(num / (2.0*double(n_family)));
        }
        // Fixed-point iteration on ε
        double eps = std::sqrt(_log_safe(2.0/alpha_f) / (2.0*double(n_family)));
        for (int it=0; it<30; ++it) {
            double m_eff = 1.0 + (double(horizon) * L) / std::max(eps, 1e-12);
            double num = 2.0*_log_safe(m_eff) + _log_safe(2.0/alpha_f);
            double eps_new = std::sqrt( num / (2.0*double(n_family)) );
            if (std::fabs(eps_new - eps) <= 1e-6 * std::max(1.0, eps)) { eps = eps_new; break; }
            eps = eps_new;
        }
        return eps;
    }

    double alpha_total_given_n_eps(double eps_target, uint64_t n_family, uint64_t /*m_family*/,
                                   uint64_t horizon, size_t n_families, double L) const override {
        n_family = _max1(n_family);
        if (n_families == 0) n_families = 1;
        if (L <= 0.0) {
            double alpha_f = 2.0 * std::exp( -2.0*double(n_family)*eps_target*eps_target );
            double alpha_tot = alpha_f * double(n_families);
            if (alpha_tot > 1.0) alpha_tot = 1.0;
            return alpha_tot;
        }
        double m_eff = 1.0 + (double(horizon) * L) / std::max(eps_target, 1e-12);
        // α_f = 2 exp( 2 ln m_eff − 2 n ε^2 )
        double alpha_f = 2.0 * std::exp( 2.0*_log_safe(m_eff) - 2.0*double(n_family)*eps_target*eps_target );
        double alpha_tot = alpha_f * double(n_families);
        if (alpha_tot > 1.0) alpha_tot = 1.0;
        return alpha_tot;
    }
};

// ---------- Legacy Hoeffding + (global) union over checkpoints ----------
class HoeffdingUnion final : public IBoundStrategy {
public:
    // NOTE: Here m_per_family is summed globally (M_total), i.e., union over all checkpoints of all families.
    Plan make_plan(double eps, double alpha_total,
                   const std::vector<uint64_t>& m_per_family,
                   uint64_t /*horizon*/,
                   size_t /*n_families*/,
                   double /*lipschitz_L*/ = 0.0) const override {
        Plan p; p.name = "Hoeffding + union over checkpoints (legacy)";
        if (eps <= 0) { p.n_required = 0; return p; }
        uint64_t M_total = 0;
        for (uint64_t m : m_per_family) M_total += _max1(m);
        M_total = _max1(M_total);
        double req = _log_safe(2.0*double(M_total)/alpha_total) / (2.0 * eps * eps);
        p.n_required = (req < 1.0 ? 1 : (uint64_t)std::ceil(req));
        return p;
    }

    // For reporting we expect m_family to actually be M_total (global).
    double epsilon_given_n(double alpha_total, uint64_t n_family, uint64_t M_total,
                           uint64_t /*horizon*/, size_t /*n_families*/, double /*L*/ = 0.0) const override {
        n_family = _max1(n_family);
        M_total  = _max1(M_total);
        return std::sqrt( _log_safe(2.0*double(M_total)/alpha_total) / (2.0*double(n_family)) );
    }

    double alpha_total_given_n_eps(double eps_target, uint64_t n_family, uint64_t M_total,
                                   uint64_t /*horizon*/, size_t /*n_families*/, double /*L*/ = 0.0) const override {
        n_family = _max1(n_family);
        M_total  = _max1(M_total);
        // α_total = 2 M_total exp(−2 n ε^2)
        double alpha_tot = 2.0 * double(M_total) * std::exp( -2.0 * double(n_family) * eps_target * eps_target );
        if (alpha_tot > 1.0) alpha_tot = 1.0;
        return alpha_tot;
    }
};

// ---------- Factory ----------
enum class StrategyKind { AUTO, DKW, RADEMACHER, LIPSCHITZ, HOEFFDING_UB };

inline const IBoundStrategy& pick_strategy(StrategyKind k, Target tgt, double lipschitz_L) {
    static DKWBound              dkw;
    static RademacherFinite      rad;
    static LipschitzRademacher   lip;
    static HoeffdingUnion        hoeff;

    if (k == StrategyKind::AUTO) {
        if (tgt == Target::SURVIVAL) return dkw;
        if (lipschitz_L > 0.0) return lip;
        return rad;
    }
    switch (k) {
        case StrategyKind::DKW:           return dkw;
        case StrategyKind::RADEMACHER:    return rad;
        case StrategyKind::LIPSCHITZ:     return lip;
        case StrategyKind::HOEFFDING_UB:  return hoeff;
        default:                          return rad;
    }
}

} // namespace stopping
