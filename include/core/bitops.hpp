#pragma once

#include <cstdint>
#include <optional>
#include <bit>
#include <random>
#include <type_traits>
#include <limits>

#if defined(__x86_64__) || defined(_M_X64)
#  include <immintrin.h>
#endif

namespace core {

namespace detail {

template <class UInt>
[[nodiscard]] inline unsigned popcount_any(UInt x) noexcept {
    static_assert(std::is_unsigned_v<UInt>);
    if constexpr (requires(UInt v) { std::popcount(v); }) {
        return static_cast<unsigned>(std::popcount(x));
    } else if constexpr (sizeof(UInt) <= sizeof(unsigned long long)) {
        return static_cast<unsigned>(std::popcount(static_cast<unsigned long long>(x)));
    } else {
        constexpr unsigned chunk_bits = std::numeric_limits<unsigned long long>::digits;
        unsigned total = 0;
        UInt value = x;
        while (value != UInt{0}) {
            total += static_cast<unsigned>(std::popcount(static_cast<unsigned long long>(value)));
            value = static_cast<UInt>(value >> chunk_bits);
        }
        return total;
    }
}

template <class UInt>
[[nodiscard]] inline unsigned countr_zero_any(UInt x) noexcept {
    static_assert(std::is_unsigned_v<UInt>);
    if constexpr (sizeof(UInt) <= sizeof(unsigned long long)) {
        return static_cast<unsigned>(std::countr_zero(static_cast<unsigned long long>(x)));
    } else {
        constexpr unsigned chunk_bits = std::numeric_limits<unsigned long long>::digits;
        unsigned tz = 0;
        UInt value = x;
        while (value != UInt{0}) {
            const unsigned long long chunk = static_cast<unsigned long long>(value);
            if (chunk != 0) {
                tz += static_cast<unsigned>(std::countr_zero(chunk));
                break;
            }
            tz += chunk_bits;
            value = static_cast<UInt>(value >> chunk_bits);
        }
        return tz;
    }
}

template <class UInt>
[[nodiscard]] inline unsigned select1_index_fallback(UInt x, unsigned kth) noexcept {
    UInt mask = x;
    while (kth--) {
        mask &= static_cast<UInt>(mask - UInt{1});
    }
    return countr_zero_any(mask);
}

} // namespace detail

/**
 * @brief Select the index of the kth set bit of x.
 * @tparam UInt Unsigned integer storage type.
 * @param x Bitset value.
 * @param kth 0-based rank among set bits (precondition: kth < popcount(x)).
 * @return Bit index within the bit width of UInt.
 */
template <class UInt>
static inline unsigned select1_index(UInt x, unsigned kth) noexcept {
    static_assert(std::is_unsigned_v<UInt>, "select1_index requires an unsigned type");
#if defined(__BMI2__)
    if constexpr (std::is_same_v<UInt, std::uint64_t>) {
        std::uint64_t onehot = _pdep_u64(1ull << kth, x);
        return static_cast<unsigned>(std::countr_zero(onehot));
    }
#endif
    return detail::select1_index_fallback<UInt>(x, kth);
}

/**
 * @brief Legacy convenience wrapper for 64-bit masks.
 */
inline unsigned select1_index_u64(std::uint64_t x, unsigned kth) noexcept {
    return select1_index<std::uint64_t>(x, kth);
}

/**
 * @brief Uniformly sample an index of a 1-bit from mask.
 * @tparam UInt Unsigned bitset type.
 * @tparam URBG RNG type; if it has uniform_index, uses it directly.
 * @param mask Bit mask; if zero, returns nullopt.
 * @param rng RNG instance.
 * @return Bit index or nullopt.
 */
template <class UInt, class URBG>
static inline std::optional<unsigned> random_setbit_index(UInt mask, URBG& rng) noexcept {
    static_assert(std::is_unsigned_v<UInt>, "random_setbit_index requires an unsigned type");
    const unsigned count = detail::popcount_any(mask);
    if (count == 0) return std::nullopt;

    unsigned kth;
    if constexpr (requires(URBG& u) { u.uniform_index(std::size_t{0}); }) {
        kth = static_cast<unsigned>(rng.uniform_index(static_cast<std::size_t>(count)));
    } else {
        std::uniform_int_distribution<unsigned> pick(0, count - 1);
        kth = pick(rng);
    }
    return select1_index<UInt>(mask, kth);
}

template <class URBG>
static inline std::optional<unsigned> random_setbit_index_u64(std::uint64_t mask, URBG& rng) noexcept {
    return random_setbit_index<std::uint64_t>(mask, rng);
}

template<class URBG, size_t B>
static inline std::optional<size_t> random_setbit_index(plf::bitset<B> mask, URBG& rng) noexcept {
    const size_t count = mask.count();
    if (count == 0) return std::nullopt;
    size_t k = std::uniform_int_distribution<size_t> pick(0, count - 1)(rng);
    size_t last_index = B - (count - k); // last possible index for kth bit
    mask = static_cast<plf::bitset<B-last_index>>(mask); // clear bits > last_index
    size_t first_index= k-1; // impossible for kth bit to be found before index k-1 or after index B-count+k
    mask <<= first_index;
    // now, first_index is 0, last_index is B - (count - k) - first_index

    mask >>= first_index; // shift out bits we know are not set
    size_t r = first_index;

    while(true) {
        auto cmp = (mask << first_index).count() <=> k;
    }

    for (size_t index=kth; index < mask.size(); index++) {
        
    }
    return std::nullopt; // should not reach here
}

template <class UInt>
[[nodiscard]] inline unsigned popcount(UInt x) noexcept {
    return detail::popcount_any(x);
}

} // namespace core
