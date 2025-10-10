#pragma once

#include "core/config.hpp"

namespace core {
namespace schelling {

/**
 * @brief Runtime threshold comparator holding p/q for Schelling dissatisfaction.
 * @note Uses cross-multiplication to avoid division in hot paths.
 */
struct PqThreshold {
    color_count_t p{1};
    color_count_t q{2};
    constexpr PqThreshold() = default;
    constexpr PqThreshold(color_count_t pp, color_count_t qq) : p(pp), q(qq) {}
    inline bool is_unhappy(color_count_t disagree, color_count_t neighbors) const noexcept { return (disagree * q) > (neighbors * p); } // branchless-friendly compare
};

/**
 * @brief Program-wide threshold state (initialize once at program startup).
 */
inline PqThreshold program_threshold{}; // defaults to 1/2
inline bool program_threshold_initialized = false;
inline bool minority_happy_ = false; // kept for tests that inspect it

/**
 * @brief Initialize program-wide threshold once; subsequent calls are no-ops.
 */
static inline void init_program_threshold(color_count_t p, color_count_t q) noexcept {
    if (!program_threshold_initialized) { 
        program_threshold.p = p; 
        program_threshold.q = q; 
        minority_happy_ = !program_threshold.is_unhappy(1, 2);
        program_threshold_initialized = true;  
    }
}

/**
 * @brief Convenience function using program-wide state.
 */
static inline bool is_unhappy(color_count_t lf, color_count_t neigh) noexcept { return program_threshold.is_unhappy(lf, neigh); }
static inline bool is_minority_happy() noexcept { return minority_happy_; }

} // namespace schelling
} // namespace core
