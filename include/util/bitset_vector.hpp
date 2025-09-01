#pragma once
// A simple dynamic bitset backed by 64-bit words. Bounds-checked gets in debug only.

#include <vector>
#include <cstdint>
#include <cstddef>
#include <cassert>
#include <cstring>

struct BitsetVector {
    std::vector<std::uint64_t> words{};
    std::size_t n_bits{0};

    BitsetVector() = default;
    explicit BitsetVector(std::size_t n) { resize(n); }

    void resize(std::size_t n) {
        n_bits = n;
        words.assign((n + 63) / 64, 0ull);
    }
    void reset() { std::fill(words.begin(), words.end(), 0ull); }

    inline void set(std::size_t i, bool val) {
        assert(i < n_bits);
        const std::size_t w = i >> 6;
        const std::size_t b = i & 63;
        const std::uint64_t mask = 1ull << b;
        if (val) words[w] |= mask; else words[w] &= ~mask;
    }
    inline bool get(std::size_t i) const {
        assert(i < n_bits);
        const std::size_t w = i >> 6;
        const std::size_t b = i & 63;
        return (words[w] >> b) & 1ull;
    }
};
