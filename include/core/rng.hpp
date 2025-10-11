#pragma once
#include <cstdint>
#include <limits>
#include <type_traits>

#include "third_party/splitmix64.hpp"
#include "third_party/xoshiro256ss.hpp"

namespace core {

// SplitMix64 wrapper (seed expander + hash)
struct SplitMix64 {
    third_party_rng::splitmix64 impl;
    explicit SplitMix64(std::uint64_t seed) : impl(seed) {}
    inline std::uint64_t next() { return impl.next(); }
};

// xoshiro256** wrapper with a familiar API
struct Xoshiro256ss {
    third_party_rng::xoshiro256ss impl;
    using result_type = std::uint64_t;
    static constexpr result_type min() noexcept { return 0u; }
    static constexpr result_type max() noexcept { return std::numeric_limits<result_type>::max(); }

    Xoshiro256ss() = default;
    explicit Xoshiro256ss(std::uint64_t seed) { seed_with_u64(seed); }

    void seed_with_u64(std::uint64_t seed) {
        third_party_rng::splitmix64 sm(seed);
        for (int i = 0; i < 4; ++i) impl.s[i] = sm.next();
        if ((impl.s[0] | impl.s[1] | impl.s[2] | impl.s[3]) == 0) impl.s[0] = 1;
    }

    static inline std::uint64_t rotl(std::uint64_t x, int k) {
        return third_party_rng::rotl(x, k);
    }

    inline std::uint64_t next_u64() { return impl.next(); }
    inline result_type operator()() { return next_u64(); }
};

inline std::uint64_t splitmix_hash(std::uint64_t x) {
    third_party_rng::splitmix64 sm(x);
    return sm.next();
}

// ---------------- Random utilities (integer-threshold selection) ----------------

// Unbiased mapping of a 64-bit URBG output to [0, n) using Lemire's
// multiply-high method with a tiny rejection loop. Works for any URBG
// that provides operator() returning a 64-bit value.
template <class URBG>
inline std::uint64_t uniform_bounded(URBG& rng, std::uint64_t n) noexcept {
    using u128 = unsigned __int128;
    std::uint64_t x = rng();
    u128 m = (u128)x * (u128)n;
    std::uint64_t l = (std::uint64_t)m;
    if (l < n) {
        const std::uint64_t t = (-n) % n;
        while (l < t) { x = rng(); m = (u128)x * (u128)n; l = (std::uint64_t)m; }
    }
    return (std::uint64_t)(m >> 64);
}

// Two-way weighted pick: returns 0 with probability w0/(w0+w1), else 1.
// Preconditions: w0+w1 > 0.
template <class URBG>
inline int weighted_pick2(URBG& rng, std::uint64_t w0, std::uint64_t w1) noexcept {
    const std::uint64_t total = w0 + w1;
    const std::uint64_t t = uniform_bounded(rng, total);
    return (t < w0) ? 0 : 1;
}

// Three-way weighted pick: returns {0,1,2} according to integer weights.
// Preconditions: w0+w1+w2 > 0.
template <class URBG>
inline int weighted_pick3(URBG& rng, std::uint64_t w0, std::uint64_t w1, std::uint64_t w2) noexcept {
    const std::uint64_t total = w0 + w1 + w2;
    const std::uint64_t t = uniform_bounded(rng, total);
    return (t < w0) ? 0 : ((t < (w0 + w1)) ? 1 : 2);
}


} // namespace core
