#pragma once

#include <cstdint>
#include <limits>
#include "core/config.hpp"

namespace core {
namespace approx {

/**
 * @brief Minimal rational type p/q with q>0.
 * @note Types match color_count_t to align with count arithmetic.
 */
struct Rat { color_count_t p; color_count_t q; };

/**
 * @brief Bracketing rationals such that lo <= tau <= hi, with q <= max_q.
 */
struct RatBounds { Rat lo; Rat hi; };

/**
 * @brief Farey/Stern–Brocot neighbors of tau with denominator <= max_q.
 * @param tau Precondition: 0 <= tau <= 1.
 * @param max_q Precondition: max_q >= 1.
 * @return {lo, hi} where lo <= tau <= hi, q_lo, q_hi <= max_q, and lo/hi are
 *         the unique best lower/upper approximations under the denominator cap.
 * @note No runtime checks; caller is responsible for honoring preconditions.
 *       Uses long double for comparison stability; integer math for mediants.
 * @complexity O(log max_q) mediant steps.
 */
static inline RatBounds farey_neighbors(double tau, color_count_t max_q) noexcept {
    // Clamp endpoints: if tau at boundaries, both bounds equal the boundary.
    if (tau <= 0.0) return RatBounds{Rat{0,1}, Rat{0,1}}; // fast path
    if (tau >= 1.0) return RatBounds{Rat{1,1}, Rat{1,1}}; // fast path
    // Invariant window [a/b, c/d] contains tau, start with [0/1, 1/1].
    color_count_t a = 0, b = 1;
    color_count_t c = 1, d = 1;
    const double x = tau;

    for (;;) {
        const color_count_t num = a + c;     // mediant numerator
        const color_count_t den = b + d;     // mediant denominator
        if (den > max_q) break;              // next mediant exceeds cap
        const double m = static_cast<double>(num) / static_cast<double>(den); // compare in FP only
        if (x > m) { a = num; b = den; }     // move lower bound up
        else if (x < m) { c = num; d = den; }// move upper bound down
        else { return {{num, den}, {num, den}}; } // exact hit
    }
    return {{a, b}, {c, d}};
}

/**
 * @brief Best (closest) rational to tau with denominator <= max_q.
 * @param tau Precondition: 0 <= tau <= 1.
 * @param max_q Precondition: max_q >= 1.
 * @return The closer of the two Farey neighbors.
 */
static inline Rat best_with_max_den(double tau, color_count_t max_q) noexcept {
    const RatBounds b = farey_neighbors(tau, max_q);
    const long double x = static_cast<long double>(tau);
    const long double dlo = x - static_cast<long double>(b.lo.p) / static_cast<long double>(b.lo.q);
    const long double dhi = static_cast<long double>(b.hi.p) / static_cast<long double>(b.hi.q) - x;
    return (dhi < dlo) ? b.hi : b.lo; // choose closer side (tie → lo)
}

/**
 * @brief Lower one-sided bound (conservative for strict '>' tests).
 * @param tau Precondition: 0 <= tau <= 1.
 * @param max_q Precondition: max_q >= 1.
 * @return Best lower approximation p/q with q <= max_q and p/q <= tau.
 */
static inline Rat lower_with_max_den(double tau, color_count_t max_q) noexcept {
    return farey_neighbors(tau, max_q).lo;
}

/**
 * @brief Upper one-sided bound (conservative for strict '<' tests).
 * @param tau Precondition: 0 <= tau <= 1.
 * @param max_q Precondition: max_q >= 1.
 * @return Best upper approximation p/q with q <= max_q and p/q >= tau.
 */
static inline Rat upper_with_max_den(double tau, color_count_t max_q) noexcept {
    return farey_neighbors(tau, max_q).hi;
}

} // namespace approx
} // namespace core
