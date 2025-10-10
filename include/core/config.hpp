// config.hpp
#pragma once
#include <cstdint>

// Compile-time configuration for core facilities.
//
// Bitset backend selection is currently plf::bitset with configurable
// storage word type and hardened mode. Extend here if additional
// backends are added later.

#ifndef CORE_BITSET_WORD_T
#define CORE_BITSET_WORD_T std::uint64_t
#endif

#ifndef CORE_BITSET_HARDENED
#define CORE_BITSET_HARDENED false
#endif


// Global hardening switch for optional runtime assertions in debug/testing code.
// Set via compile flag: -DCORE_HARDENED=1
#ifndef CORE_HARDENED
#define CORE_HARDENED 0
#endif

#if CORE_HARDENED
#include <stdexcept>
#define CORE_ASSERT_H(cond, msg) do { if(!(cond)) throw std::runtime_error(msg); } while(0)
#else
#define CORE_ASSERT_H(cond, msg) do { } while(0)
#endif

// Default color/count type for Schelling counts and related combinatorics.
#ifndef CORE_COLOR_COUNT_T
#define CORE_COLOR_COUNT_T std::uint64_t
#endif
// color_count_t is used widely in core and callers; keep a global alias
// for compatibility and also provide a namespaced alias.
using color_count_t = CORE_COLOR_COUNT_T;
namespace core { using color_count_t = ::color_count_t; }

// Lightweight ANSI color tokens for optional debug/printing.
// Keep simple pointers to string literals to avoid extra headers.
namespace core { namespace config {
inline constexpr const char* col0  = "\x1b[34m"; // blue for color 0
inline constexpr const char* col1  = "\x1b[31m"; // red  for color 1
inline constexpr const char* reset = "\x1b[0m";  // reset
} }

// Test macro fallbacks (doctest) for non-test builds
#ifndef DOCTEST_VERSION
#ifndef CAPTURE
#define CAPTURE(x) do { } while(0)
#endif
#ifndef REQUIRE
#define REQUIRE(x) do { } while(0)
#endif
#endif
