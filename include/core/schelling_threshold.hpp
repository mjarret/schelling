#pragma once

#include <cstdint>
#include "core/config.hpp"

namespace core {
namespace schelling {

// Runtime threshold comparator holding P/Q
struct PqThreshold {
    color_count_t p{1};
    color_count_t q{2};
    constexpr PqThreshold() = default;
    constexpr PqThreshold(color_count_t pp, color_count_t qq) : p(pp), q(qq) {}
    inline bool is_unhappy(color_count_t lf, color_count_t neigh) const noexcept { return (lf * q) > (p * neigh); }
};

// Program-wide threshold state (initialize once at program startup).
inline PqThreshold program_threshold{}; // defaults to 1/2
inline bool program_threshold_initialized = false;

// Initialize threshold once; subsequent calls are no-ops by design.
static inline void init_program_threshold(color_count_t p, color_count_t q) noexcept {
    if (!program_threshold_initialized) { program_threshold.p = p; program_threshold.q = q; program_threshold_initialized = true; }
}
static inline const PqThreshold& get_program_threshold() noexcept { return program_threshold; }

// Convenience free function using program-wide state
static inline bool is_unhappy(color_count_t lf, color_count_t neigh) noexcept { return program_threshold.is_unhappy(lf, neigh); }

} // namespace schelling
} // namespace core
