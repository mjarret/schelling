#pragma once

#include <cstdint>
#include <optional>
#include <bit>
#include <random>

#if defined(__x86_64__) || defined(_M_X64)
#  include <immintrin.h>
#endif

namespace core {

// Select the index (0..63) of the kth set bit in x (0-based order among 1-bits).
// Precondition: kth < popcount(x). No runtime checks.
static inline unsigned select1_index_u64(std::uint64_t x, unsigned kth) noexcept {
#if defined(__BMI2__)
    // BMI2: one instruction to scatter kth into position, then ctz
    std::uint64_t onehot = _pdep_u64(1ull << kth, x);
    return static_cast<unsigned>(std::countr_zero(onehot));
#else
    // Portable fallback: descend 32/16/8-bit chunks by popcount, then finish within byte
    unsigned base = 0;
    std::uint32_t lo32 = static_cast<std::uint32_t>(x);
    unsigned pc = std::popcount(lo32);
    if (kth >= pc) { kth -= pc; x >>= 32; base = 32; } else { x = lo32; }

    std::uint16_t lo16 = static_cast<std::uint16_t>(x);
    pc = std::popcount(lo16);
    if (kth >= pc) { kth -= pc; x >>= 16; base += 16; } else { x = lo16; }

    std::uint8_t lo8 = static_cast<std::uint8_t>(x);
    pc = std::popcount(lo8);
    if (kth >= pc) { kth -= pc; x >>= 8; base += 8; } else { x = lo8; }

    std::uint8_t b = static_cast<std::uint8_t>(x);
    while (kth--) b = static_cast<std::uint8_t>(b & (b - 1));
    return base + static_cast<unsigned>(std::countr_zero(static_cast<unsigned>(b)));
#endif
}

// Uniformly sample a 1-bit index from mask; returns nullopt when mask==0.
template <class URBG>
static inline std::optional<unsigned> random_setbit_index_u64(std::uint64_t mask, URBG& rng) noexcept {
    const unsigned m = std::popcount(mask);
    if (m == 0) return std::nullopt;
    unsigned kth;
    if constexpr (requires(URBG& u) { u.uniform_index(std::size_t{0}); }) {
        kth = static_cast<unsigned>(rng.uniform_index(m));
    } else {
        std::uniform_int_distribution<unsigned> pick(0, m - 1);
        kth = pick(rng);
    }
    return select1_index_u64(mask, kth);
}

} // namespace core
