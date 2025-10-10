// xoshiro256** — reference implementation (Public Domain / CC0)
// Authors: David Blackman and Sebastiano Vigna (see prng.di.unimi.it)
#pragma once
#include <cstdint>

namespace third_party_rng {

static inline std::uint64_t rotl(const std::uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
}

struct xoshiro256ss {
    std::uint64_t s[4] { 0, 0, 0, 0 };
    inline std::uint64_t next() {
        const std::uint64_t result = rotl(s[1] * 5ULL, 7) * 9ULL;
        const std::uint64_t t = s[1] << 17;
        s[2] ^= s[0];
        s[3] ^= s[1];
        s[1] ^= s[2];
        s[0] ^= s[3];
        s[2] ^= t;
        s[3] = rotl(s[3], 45);
        return result;
    }
};

} // namespace third_party_rng

