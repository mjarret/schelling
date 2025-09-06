// Lightweight performance/branchless helpers. No runtime asserts by default.
#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

// Attributes (portable fallbacks)
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

// Restrict-like qualifier for pointers (best-effort)
#if defined(__GNUC__) || defined(__clang__)
#  define PERF_RESTRICT __restrict__
#else
#  define PERF_RESTRICT
#endif

// ASSUME: Promise a condition holds. UB if false; use with caution.
#if defined(__clang__)
#  define PERF_ASSUME(cond) __builtin_assume(cond)
#elif defined(__GNUC__)
// GCC lacks builtin_assume; use unreachable on false path
#  define PERF_ASSUME(cond) do { if (!(cond)) __builtin_unreachable(); } while (0)
#elif defined(_MSC_VER)
#  define PERF_ASSUME(cond) __assume(cond)
#else
#  define PERF_ASSUME(cond) ((void)0)
#endif

// Branchless select for integral and enum types
template <class T>
PERF_ALWAYS_INLINE constexpr std::enable_if_t<std::is_integral_v<T> || std::is_enum_v<T>, T>
branchless_select(bool cond, T a, T b) noexcept {
    using U = std::make_unsigned_t<std::underlying_type_t<T>>;
    U mask = static_cast<U>(-static_cast<std::make_signed_t<U>>(cond)); // all 1s if cond, else 0
    U ua = static_cast<U>(a);
    U ub = static_cast<U>(b);
    return static_cast<T>(ub ^ (mask & (ua ^ ub)));
}

// Branchless min/max for unsigned integers
template <class T>
PERF_ALWAYS_INLINE constexpr std::enable_if_t<std::is_unsigned_v<T>, T>
branchless_min(T a, T b) noexcept {
    T diff = a ^ ((a ^ b) & -(a > b));
    return diff;
}

template <class T>
PERF_ALWAYS_INLINE constexpr std::enable_if_t<std::is_unsigned_v<T>, T>
branchless_max(T a, T b) noexcept {
    T diff = b ^ ((a ^ b) & -(a > b));
    return diff;
}

// Prefetch convenience wrappers
PERF_ALWAYS_INLINE inline void prefetch_read(const void* p, int locality = 3) noexcept {
    PERF_PREFETCH(p, 0, locality);
}

PERF_ALWAYS_INLINE inline void prefetch_write(const void* p, int locality = 3) noexcept {
    PERF_PREFETCH(p, 1, locality);
}

