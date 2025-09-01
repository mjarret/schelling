#pragma once
#include <cstdint>

struct SplitMix64 {
    uint64_t state;
    explicit SplitMix64(uint64_t seed = 0x9e3779b97f4a7c15ULL) : state(seed) {}
    inline uint64_t next_u64() {
        uint64_t z = (state += 0x9e3779b97f4a7c15ULL);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }
    inline uint64_t uniform_u64(uint64_t n) { // [0, n)
        uint64_t x, t = (~uint64_t(0) / n) * n;
        do { x = next_u64(); } while (x >= t);
        return x % n;
    }
};
