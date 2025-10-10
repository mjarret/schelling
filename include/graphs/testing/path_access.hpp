// path_access.hpp — test-only accessors for graphs::Path
#pragma once

// Define SCHELLING_TEST_ACCESSORS=1 in test builds to enable these accessors.
// When disabled, this header provides only forward declarations.

#if SCHELLING_TEST_ACCESSORS

#include "graphs/path.hpp"
#include "core/plf_bitset.h"

namespace graphs { namespace test {

template<std::size_t B>
struct PathAccess {
    static const plf::bitset<B + 4>& raw_occ(const ::Path<B>& p)             { return p.occ_.raw(); }
    static const plf::bitset<B + 4>& raw_colors(const ::Path<B>& p)          { return p.col_.raw(); }
    static const plf::bitset<B + 4>& raw_unhappy_cache(const ::Path<B>& p)   { return p.unhappy_mask_cache_.raw(); }
};

// Optional free-function helpers
template<std::size_t B>
inline const plf::bitset<B + 4>& raw_occ(const ::Path<B>& p) { return PathAccess<B>::raw_occ(p); }

template<std::size_t B>
inline const plf::bitset<B + 4>& raw_colors(const ::Path<B>& p) { return PathAccess<B>::raw_colors(p); }

template<std::size_t B>
inline const plf::bitset<B + 4>& raw_unhappy_cache(const ::Path<B>& p) { return PathAccess<B>::raw_unhappy_cache(p); }

} } // namespace graphs::test

#else // SCHELLING_TEST_ACCESSORS

namespace graphs { namespace test {
template<std::size_t B>
struct PathAccess; // forward decl only when accessors are disabled
} } // namespace graphs::test

#endif // SCHELLING_TEST_ACCESSORS

