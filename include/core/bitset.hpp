// bitset.hpp
#pragma once

#include "core/config.hpp"
#include "core/plf_bitset.h"

namespace core {

// Static bitset alias used across the workspace.
// Backend: plf::bitset with configurable word type and hardened flag.
template <std::size_t N>
using bitset = plf::bitset<N, CORE_BITSET_WORD_T, CORE_BITSET_HARDENED>;

} // namespace core

