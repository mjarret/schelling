// stopping_hoeffding.hpp
#pragma once
#include <cstdint>
#include <cmath>
#include <vector>
#include <algorithm>
#include <limits>

namespace hoeffding {

/**
 * Compute required per-component sample size to guarantee, via Hoeffding and
 * a union bound over M components, that the empirical mean of each bounded
 * variable in [0,1] is within ±eps of its true mean with probability ≥ 1-alpha.
 *
 * n_req >= (1/(2*eps^2)) * ln(2*M/alpha).
 */
inline uint64_t required_runs(double eps, double alpha, uint64_t M_components) {
    if (eps <= 0.0) return std::numeric_limits<uint64_t>::max();
    if (alpha <= 0.0) alpha = 1e-300; // avoid log(0)
    if (M_components == 0) M_components = 1;
    const double num = std::log((2.0 * double(M_components)) / alpha);
    const double den = 2.0 * eps * eps;
    double req = num / den;
    if (req < 1.0) req = 1.0;
    return static_cast<uint64_t>(std::ceil(req));
}

/**
 * A plan covering multiple "families" of components. In our use:
 *   family = parameter combo, components = checkpoints on the curve.
 * We form a *global* guarantee across ALL families by taking M_total as the
 * sum of per-family component counts and applying a union bound across M_total.
 */
struct MultiFamilyPlan {
    double eps{0.0};
    double alpha{1e-4};
    uint64_t M_total{0};       // total number of components across all families
    uint64_t n_required{0};    // per-component requirement

    // Construct from eps/alpha and per-family component counts.
    explicit MultiFamilyPlan(double eps_, double alpha_, const std::vector<uint64_t>& m_per_family)
    : eps(eps_), alpha(alpha_) {
        M_total = 0;
        for (auto m : m_per_family) M_total += (m == 0 ? 1 : m);
        n_required = required_runs(eps, alpha, M_total);
    }
};

/**
 * Progress view for a given family: the minimum count across that family's components.
 * This is the binding bottleneck count.
 */
struct FamilyProgress {
    uint64_t min_count{0};
    uint64_t components{0};
};

/**
 * Aggregate "how far along are we" across all families:
 *  - satisfied = true if every family's min_count >= n_required
 *  - total_min = sum of each family's min_count (useful for progress bars)
 */
struct GlobalProgress {
    bool satisfied{false};
    uint64_t total_min{0};
};

/**
 * Compute the global progress. The `get_count(family, component)` accessor
 * must return the current *count* for that component (how many runs contributed).
 */
template<class CountAccessor>
inline GlobalProgress progress(const MultiFamilyPlan& plan,
                               const std::vector<uint64_t>& m_per_family,
                               CountAccessor get_count) {
    GlobalProgress g{};
    bool all_ok = true;
    uint64_t sum_min = 0;
    const size_t F = m_per_family.size();
    for (size_t f = 0; f < F; ++f) {
        const uint64_t m = (m_per_family[f] == 0 ? 1 : m_per_family[f]);
        uint64_t minc = std::numeric_limits<uint64_t>::max();
        for (uint64_t k = 0; k < m; ++k) {
            const uint64_t c = get_count(f, k);
            if (c < minc) minc = c;
        }
        if (minc == std::numeric_limits<uint64_t>::max()) minc = 0;
        if (minc < plan.n_required) all_ok = false;
        sum_min += minc;
    }
    g.satisfied = all_ok;
    g.total_min = sum_min;
    return g;
}

} // namespace hoeffding
