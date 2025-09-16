#pragma once
#include <vector>
#include <cstdint>
#include <span>

namespace core {

/**
 * @brief Fixed-bit packed vector storing elements with BITS bits each.
 * @tparam BITS Number of bits per stored element in [1, 32].
 */
template <unsigned BITS>
class BitPackedVector {
    static_assert(BITS >= 1 && BITS <= 64, "BITS must be in [1,64]");
    using word_t = std::uint64_t;
    static constexpr unsigned WORD_BITS = 64;

    std::size_t n_ = 0;
    std::vector<word_t> data_;

public:
    using word_type = std::uint64_t;
    /**
     * @brief Default constructor.
     */
    BitPackedVector() = default;

    /**
     * @brief Construct a packed vector of size n with zero-initialized storage.
     * @param n Number of elements.
     */
    explicit BitPackedVector(std::size_t n) { resize(n); }

    /**
     * @brief Resize to hold n elements and zero-initialize underlying words.
     * @param n Number of elements.
     */
    void resize(std::size_t n) {
        n_ = n;
        const std::size_t total_bits = n * BITS;
        const std::size_t words = (total_bits + WORD_BITS - 1) / WORD_BITS;
        data_.assign(words, 0);
    }

    /**
     * @brief Number of elements stored.
     * @return Element count.
     */
    std::size_t size() const { return n_; }
    /**
     * @brief Number of dits stored (alias for size()).
     */
    std::size_t dits() const { return n_; }
    /**
     * @brief Total number of bits in the logical stream.
     */
    std::size_t bit_length() const { return n_ * BITS; }

    /**
     * @brief Get the BITS-wide value at logical index i.
     * @param i Element index.
     * @return Extracted value as 32-bit unsigned integer.
     */
    std::uint64_t get(std::size_t i) const {
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
            return low | (high << first);
        }
    }

    /**
     * @brief Set the BITS-wide value at logical index i.
     * @param i Element index.
     * @param v Value to set (lower BITS bits are used).
     */
    void set(std::size_t i, std::uint64_t v) {
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

    /**
     * @brief Expose underlying 64-bit words for bitwise operations.
     *        For BITS==1, this is a packed bitstring view.
     */
    std::span<const word_type> words() const noexcept { return std::span<const word_type>(data_.data(), data_.size()); }
    std::span<word_type>       words()       noexcept { return std::span<word_type>(data_.data(), data_.size()); }

    /**
     * @brief Zero all underlying words.
     */
    void clear_words() noexcept { for (auto& w : data_) w = 0ULL; }
};

// Alias matching requested terminology
template <unsigned d>
using DitString = BitPackedVector<d>;

template <std::uint64_t K>
struct BitsForK {
    static constexpr std::uint64_t states = K + 1;
    static constexpr unsigned value =
        (states <=      2ull) ? 1 :
        (states <=      4ull) ? 2 :
        (states <=      8ull) ? 3 :
        (states <=     16ull) ? 4 :
        (states <=     32ull) ? 5 :
        (states <=     64ull) ? 6 :
        (states <=    128ull) ? 7 :
        (states <=    256ull) ? 8 :
        (states <=    512ull) ? 9 :
        (states <=   1024ull) ? 10 : 32;
};

}
