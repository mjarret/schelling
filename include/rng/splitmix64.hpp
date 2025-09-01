#pragma once
// Simple SplitMix64 PRNG (fast, stateless stepping), header-only.

#include <cstdint>
#include <limits>
#include <random>

struct SplitMix64 {
    using result_type = std::uint64_t;
    std::uint64_t state;

    explicit SplitMix64(std::uint64_t seed = 0x9E3779B97F4A7C15ULL) : state(seed) {}

    static constexpr result_type min() { return std::numeric_limits<result_type>::min(); }
    static constexpr result_type max() { return std::numeric_limits<result_type>::max(); }

    inline std::uint64_t next_u64() {
        std::uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }
    inline std::uint32_t next_u32() { return static_cast<std::uint32_t>(next_u64() >> 32); }

    inline double next_unit_double() {
        // 53-bit mantissa
        const std::uint64_t r = next_u64();
        return (r >> 11) * (1.0 / (1ull << 53));
    }

    // Uniform in [0, n)
    inline std::size_t uniform_index(std::size_t n) {
        // Rejection sampling to avoid modulo bias.
        const std::uint64_t limit = (std::numeric_limits<std::uint64_t>::max() / n) * n;
        std::uint64_t r;
        do { r = next_u64(); } while (r >= limit);
        return static_cast<std::size_t>(r % n);
    }
};