// splitmix64.hpp — reference SplitMix64 (Public Domain / CC0)
// Source algorithm: Sebastiano Vigna (see prng.di.unimi.it)
#pragma once
#include <cstdint>

namespace third_party_rng {

struct splitmix64 {
    std::uint64_t x;
    explicit splitmix64(std::uint64_t seed) : x(seed) {}
    inline std::uint64_t next() {
        std::uint64_t z = (x += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }
};

} // namespace third_party_rng

