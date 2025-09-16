#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#if defined(__GNUC__) || defined(__clang__)
#  define PERF_HOT   __attribute__((hot))
#  define PERF_COLD  __attribute__((cold))
#  define PERF_NOINLINE __attribute__((noinline))
#  define PERF_ALWAYS_INLINE inline __attribute__((always_inline))
#  define PERF_LIKELY(x)   (__builtin_expect(!!(x), 1))
#  define PERF_UNLIKELY(x) (__builtin_expect(!!(x), 0))
#  define PERF_PREFETCH(addr, rw, locality) __builtin_prefetch((addr), (rw), (locality))
#else
#  define PERF_HOT
#  define PERF_COLD
#  define PERF_NOINLINE
#  define PERF_ALWAYS_INLINE inline
#  define PERF_LIKELY(x)   (x)
#  define PERF_UNLIKELY(x) (x)
#  define PERF_PREFETCH(addr, rw, locality) ((void)0)
#endif

#if defined(__GNUC__) || defined(__clang__)
#  define PERF_RESTRICT __restrict__
#else
#  define PERF_RESTRICT
#endif

#if defined(__clang__)
#  define PERF_ASSUME(cond) __builtin_assume(cond)
#elif defined(__GNUC__)
#  define PERF_ASSUME(cond) do { if (!(cond)) __builtin_unreachable(); } while (0)
#elif defined(_MSC_VER)
#  define PERF_ASSUME(cond) __assume(cond)
#else
#  define PERF_ASSUME(cond) ((void)0)
#endif

/**
 * @brief Branchless conditional selection for integral and enum types.
 * @tparam T Unsigned/signed integral or enum type.
 * @param cond Condition determining selection.
 * @param a Value when cond is true.
 * @param b Value when cond is false.
 * @return Selected value without a data-dependent branch.
 */
template <class T>
PERF_ALWAYS_INLINE constexpr std::enable_if_t<std::is_integral_v<T> || std::is_enum_v<T>, T>
branchless_select(bool cond, T a, T b) noexcept {
    using U = std::make_unsigned_t<std::underlying_type_t<T>>;
    U mask = static_cast<U>(-static_cast<std::make_signed_t<U>>(cond));
    U ua = static_cast<U>(a);
    U ub = static_cast<U>(b);
    return static_cast<T>(ub ^ (mask & (ua ^ ub)));
}

/**
 * @brief Branchless minimum for unsigned integers.
 * @tparam T Unsigned integral type.
 * @param a First operand.
 * @param b Second operand.
 * @return Minimum of a and b without a branch.
 */
template <class T>
PERF_ALWAYS_INLINE constexpr std::enable_if_t<std::is_unsigned_v<T>, T>
branchless_min(T a, T b) noexcept {
    T diff = a ^ ((a ^ b) & -(a > b));
    return diff;
}

/**
 * @brief Branchless maximum for unsigned integers.
 * @tparam T Unsigned integral type.
 * @param a First operand.
 * @param b Second operand.
 * @return Maximum of a and b without a branch.
 */
template <class T>
PERF_ALWAYS_INLINE constexpr std::enable_if_t<std::is_unsigned_v<T>, T>
branchless_max(T a, T b) noexcept {
    T diff = b ^ ((a ^ b) & -(a > b));
    return diff;
}

/**
 * @brief Prefetch memory for read access.
 * @param p Address to prefetch.
 * @param locality Locality hint [0..3].
 */
PERF_ALWAYS_INLINE void prefetch_read(const void* p, int locality = 3) noexcept {
    PERF_PREFETCH(p, 0, locality);
}

/**
 * @brief Prefetch memory for write access.
 * @param p Address to prefetch.
 * @param locality Locality hint [0..3].
 */
PERF_ALWAYS_INLINE void prefetch_write(const void* p, int locality = 3) noexcept {
    PERF_PREFETCH(p, 1, locality);
}
