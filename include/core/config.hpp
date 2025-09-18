#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

/**
 * @file core/config.hpp
 * @brief Common type aliases and compile-time constraints for Schelling code.
 */

/**
 * @brief Global index type for vertices.
 */
#ifndef SCHELLING_INDEX_T
using index_t = std::size_t;
#else
using index_t = SCHELLING_INDEX_T;
#endif

/**
 * @brief Minimal-width color index type for compile-time K.
 */
template <std::uint64_t K>
using color_index_for = std::conditional_t<(K <= 0xFFull),  std::uint8_t,
                          std::conditional_t<(K <= 0xFFFFull), std::uint16_t,
                          std::conditional_t<(K <= 0xFFFFFFFFull), std::uint32_t,
                          std::uint64_t>>>;

/**
 * @brief Runtime color index type.
 */
#ifndef SCHELLING_COLOR_INDEX_T
using color_index_t = std::uint32_t;
#else
using color_index_t = SCHELLING_COLOR_INDEX_T;
#endif

/**
 * @brief Count type for color populations and denominators.
 */
#ifndef SCHELLING_COLOR_COUNT_T
using color_count_t = std::uint64_t;
#else
using color_count_t = SCHELLING_COLOR_COUNT_T;
#endif

/**
 * @brief Type for frustration/penalty tallies.
 */
#ifndef SCHELLING_FRUSTRATION_T
using frustration_t = std::uint64_t;
#else
using frustration_t = SCHELLING_FRUSTRATION_T;
#endif

// Compile-time constraints: unsigned and non-narrowing relationships.
static_assert(std::is_unsigned_v<index_t>, "index_t must be an unsigned integral type");
static_assert(std::is_unsigned_v<color_index_t>, "color_index_t must be an unsigned integral type");
static_assert(std::is_unsigned_v<color_count_t>, "color_count_t must be an unsigned integral type");
static_assert(std::is_unsigned_v<frustration_t>, "frustration_t must be an unsigned integral type");
static_assert(sizeof(color_count_t) >= sizeof(index_t), "color_count_t should be wide enough to hold vertex counts");
static_assert(sizeof(frustration_t) >= sizeof(color_count_t), "frustration_t should be >= color_count_t width");
