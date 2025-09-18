#pragma once

#include <cstdint>
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
    inline bool is_unhappy(color_count_t lf, color_count_t neigh) const noexcept { return (lf * q) > (p * neigh); } // branchless-friendly compare
};

/**
 * @brief Program-wide threshold state (initialize once at program startup).
 */
inline PqThreshold program_threshold{}; // defaults to 1/2
inline bool program_threshold_initialized = false;

/**
 * @brief Initialize program-wide threshold once; subsequent calls are no-ops.
 */
static inline void init_program_threshold(color_count_t p, color_count_t q) noexcept {
    if (!program_threshold_initialized) { program_threshold.p = p; program_threshold.q = q; minority_okay_ = (q >= (p << 1)); program_threshold_initialized = true;  }
}

/**
 * @brief Access the program-wide threshold.
 */
static inline const PqThreshold& get_program_threshold() noexcept { return program_threshold; }

/**
 * @brief Convenience function using program-wide state.
 */
static inline bool is_unhappy(color_count_t lf, color_count_t neigh) noexcept { return program_threshold.is_unhappy(lf, neigh); }

static inline bool is_minority_ok() noexcept { return minority_okay_; }

static bool minority_okay_ = true; // default to minority okay (tau <= 1/2)

} // namespace schelling
} // namespace core
