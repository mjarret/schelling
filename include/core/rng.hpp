// rng.hpp
#pragma once
#include <array>
#include <cstdint>
#include <random>
#include <type_traits>

namespace core {

// SplitMix64 for robust seeding
struct SplitMix64 {
    std::uint64_t state;
    explicit SplitMix64(std::uint64_t seed) : state(seed) {}
    inline std::uint64_t next() {
        std::uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }
};

// xoshiro256** engine (fast, high-quality for simulations)
struct Xoshiro256ss {
    std::array<std::uint64_t,4> s{0,0,0,0};

    Xoshiro256ss() = default;
    explicit Xoshiro256ss(std::uint64_t seed) { seed_with_u64(seed); }

    template <class SeedSeq>
    void seed(SeedSeq& seq) {
        std::uint32_t buf[8]{};
        seq.generate(std::begin(buf), std::end(buf));
        // Pack 8x32 into 4x64
        for (int i=0;i<4;++i) {
            s[i] = (std::uint64_t(buf[2*i]) << 32) | buf[2*i+1];
            if (s[i] == 0) s[i] = 0x9E3779B97F4A7C15ULL ^ (0xBF58476D1CE4E5B9ULL * (i+1));
        }
        if ((s[0]|s[1]|s[2]|s[3]) == 0) { // all zero is an invalid state
            seed_with_u64(0xDEADBEEFCAFEBABEULL);
        }
    }

    void seed_with_u64(std::uint64_t seed) {
        SplitMix64 sm(seed);
        for (int i=0;i<4;++i) {
            s[i] = sm.next();
        }
        if ((s[0]|s[1]|s[2]|s[3]) == 0) {
            s[0] = 1;
        }
    }

    static inline std::uint64_t rotl(const std::uint64_t x, int k) {
        return (x << k) | (x >> (64 - k));
    }

    inline std::uint64_t next_u64() {
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

    inline double uniform01() {
        // 53-bit precision in [0,1)
        return (next_u64() >> 11) * (1.0/9007199254740992.0);
    }

    inline std::uint64_t uniform_u64(std::uint64_t bound) {
        // unbiased modulo reduction
        std::uint64_t x, m = -bound % bound;
        do { x = next_u64(); } while (x < m);
        return x % bound;
    }

    inline std::size_t uniform_index(std::size_t n) {
        return static_cast<std::size_t>(uniform_u64(static_cast<std::uint64_t>(n)));
    }
};

inline std::uint64_t splitmix_hash(std::uint64_t x) {
    SplitMix64 sm(x);
    return sm.next();
}

} // namespace core
