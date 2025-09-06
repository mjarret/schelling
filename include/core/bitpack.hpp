// bitpack.hpp
#pragma once
#include <vector>
#include <cstdint>
#include <cassert>

namespace core {

template <unsigned BITS>
class BitPackedVector {
    static_assert(BITS >= 1 && BITS <= 32, "BITS must be in [1,32]");
    using word_t = std::uint64_t;
    static constexpr unsigned WORD_BITS = 64;

    std::size_t n_ = 0;
    std::vector<word_t> data_;

public:
    BitPackedVector() = default;
    explicit BitPackedVector(std::size_t n) { resize(n); }

    void resize(std::size_t n) {
        n_ = n;
        const std::size_t total_bits = n * BITS;
        const std::size_t words = (total_bits + WORD_BITS - 1) / WORD_BITS;
        data_.assign(words, 0);
    }

    std::size_t size() const { return n_; }

    std::uint32_t get(std::size_t i) const {
        assert(i < n_);
        const std::size_t bit = i * BITS;
        const std::size_t idx = bit / WORD_BITS;
        const unsigned     off = bit % WORD_BITS;
        const word_t mask = (BITS == 64) ? ~word_t(0) : ((word_t(1) << BITS) - 1);
        if (off + BITS <= WORD_BITS) {
            return (data_[idx] >> off) & mask;
        } else {
            const unsigned first = WORD_BITS - off;
            const word_t low  = data_[idx] >> off;
            const word_t high = data_[idx+1] & ((word_t(1) << (BITS - first)) - 1);
            return static_cast<std::uint32_t>(low | (high << first));
        }
    }

    void set(std::size_t i, std::uint32_t v) {
        assert(i < n_);
        const std::size_t bit = i * BITS;
        const std::size_t idx = bit / WORD_BITS;
        const unsigned     off = bit % WORD_BITS;
        const word_t mask = (BITS == 64) ? ~word_t(0) : ((word_t(1) << BITS) - 1);
        v &= mask;
        if (off + BITS <= WORD_BITS) {
            const word_t m = (mask << off);
            data_[idx] = (data_[idx] & ~m) | (word_t(v) << off);
        } else {
            const unsigned first = WORD_BITS - off;
            const word_t m0 = ((word_t(1) << first) - 1) << off;
            data_[idx] = (data_[idx] & ~m0) | ((word_t(v) & ((word_t(1)<<first)-1)) << off);
            const unsigned rest = BITS - first;
            const word_t   m1 = (word_t(1) << rest) - 1;
            data_[idx+1] = (data_[idx+1] & ~m1) | (word_t(v) >> first);
        }
    }
};

template <std::uint64_t K>
struct BitsForK {
    static constexpr unsigned value =
        (K <= 2) ? 1 :
        (K <= 4) ? 2 :
        (K <= 8) ? 3 :
        (K <= 16) ? 4 :
        (K <= 32) ? 5 :
        (K <= 64) ? 6 :
        (K <= 128) ? 7 :
        (K <= 256) ? 8 :
        (K <= 512) ? 9 :
        (K <= 1024) ? 10 : 32;
};

} // namespace core
