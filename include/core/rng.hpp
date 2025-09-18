#pragma once
#include <array>
#include <cstdint>
#include <limits>
#include <random>
#include <type_traits>

namespace core {

/**
 * @brief SplitMix64 generator for robust seeding.
 * @note Used to expand a 64-bit seed into high-entropy state for xoshiro.
 */
struct SplitMix64 {
    std::uint64_t state;
    /**
     * @brief Construct with 64-bit seed.
     * @param seed Initial state.
     */
    explicit SplitMix64(std::uint64_t seed) : state(seed) {}
    /**
     * @brief Generate next 64-bit value.
     * @return Next value.
     */
    inline std::uint64_t next() {
        std::uint64_t z = (state += 0x9E3779B97F4A7C15ULL); // 64-bit golden ratio increment
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;       // mix step 1
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;       // mix step 2
        return z ^ (z >> 31);                               // final avalanche
    }
};

/**
 * @brief xoshiro256** RNG (fast, high-quality for simulations).
 */
struct Xoshiro256ss {
    std::array<std::uint64_t,4> s{0,0,0,0};
    using result_type = std::uint64_t;
    static constexpr result_type min() noexcept { return 0u; }
    static constexpr result_type max() noexcept { return std::numeric_limits<result_type>::max(); }
    /**
     * @brief Default constructor (zero state).
     */
    Xoshiro256ss() = default;
    /**
     * @brief Construct and seed from a 64-bit value.
     * @param seed Seed value.
     */
    explicit Xoshiro256ss(std::uint64_t seed) { seed_with_u64(seed); }

    template <class SeedSeq>
    /**
     * @brief Seed the engine from a SeedSeq.
     * @tparam SeedSeq Seed sequence type with generate(first,last).
     * @param seq Seed sequence instance.
     */
    void seed(SeedSeq& seq) {
        std::uint32_t buf[8]{};
        seq.generate(std::begin(buf), std::end(buf));
        for (int i=0;i<4;++i) {
            s[i] = (std::uint64_t(buf[2*i]) << 32) | buf[2*i+1];
            if (s[i] == 0) s[i] = 0x9E3779B97F4A7C15ULL ^ (0xBF58476D1CE4E5B9ULL * (i+1));
        }
        if ((s[0]|s[1]|s[2]|s[3]) == 0) {
            seed_with_u64(0xDEADBEEFCAFEBABEULL);
        }
    }

    /**
     * @brief Seed using SplitMix64 expansion from 64-bit seed.
     * @param seed Seed value.
     */
    void seed_with_u64(std::uint64_t seed) {
        SplitMix64 sm(seed);
        for (int i=0;i<4;++i) {
            s[i] = sm.next();
        }
        if ((s[0]|s[1]|s[2]|s[3]) == 0) {
            s[0] = 1;
        }
    }

    /**
     * @brief Rotate-left 64-bit value by k bits.
     * @param x Value to rotate.
     * @param k Rotation amount.
     * @return Rotated value.
     */
    static inline std::uint64_t rotl(const std::uint64_t x, int k) {
        return (x << k) | (x >> (64 - k)); // rotates are single uop on x86-64
    }

    /**
     * @brief Generate next 64-bit output.
     * @return Next 64-bit value.
     */
    inline std::uint64_t next_u64() {
        const std::uint64_t result = rotl(s[1] * 5ULL, 7) * 9ULL; // xoshiro** output function
        const std::uint64_t t = s[1] << 17;

        s[2] ^= s[0];
        s[3] ^= s[1];
        s[1] ^= s[2];
        s[0] ^= s[3];

        s[2] ^= t;
        s[3] = rotl(s[3], 45);

        return result;
    }

    inline result_type operator()() { return next_u64(); }

    /**
     * @brief Uniform double in [0,1).
     * @return Double sample.
     */
    inline double uniform01() {
        return (next_u64() >> 11) * (1.0/9007199254740992.0); // 53 bits -> [0,1) exactly representable
    }

    /**
     * @brief Uniform integer in [0, bound).
     * @param bound Upper bound (exclusive).
     * @return Sampled value without modulo bias.
     */
    inline std::uint64_t uniform_u64(std::uint64_t bound) {
        std::uint64_t x, m = -bound % bound;                 // rejection threshold to avoid modulo bias
        do { x = next_u64(); } while (x < m);                 // branchless-friendly loop on typical RNG
        return x % bound;
    }

    /**
     * @brief Uniform index in [0, n).
     * @param n Number of items.
     * @return Index sample.
     */
    inline std::size_t uniform_index(std::size_t n) {
        return uniform_u64(n);
    }
};

/**
 * @brief One-step SplitMix64 hash of a 64-bit value.
 * @param x Input value.
 * @return Hashed value.
 */
inline std::uint64_t splitmix_hash(std::uint64_t x) {
    SplitMix64 sm(x);
    return sm.next();
}

}
