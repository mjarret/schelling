#pragma once
#include <cstdint>
#include <limits>

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

} // namespace core

