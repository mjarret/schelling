// lollipop_access.hpp — test-only accessors for graphs::LollipopGraph
#pragma once

#if SCHELLING_TEST_ACCESSORS

#include "graphs/lollipop.hpp"

namespace graphs { namespace test {

template<std::size_t CS, std::size_t PL>
struct LollipopAccess {
    static std::size_t bridge_index(const graphs::LollipopGraph<CS, PL>& g) { return g.bridge_index_(); }
    static std::size_t c0(const graphs::LollipopGraph<CS, PL>& g) { return g.clique_.count_by_color(false); }
    static std::size_t c1(const graphs::LollipopGraph<CS, PL>& g) { return g.clique_.count_by_color(true); }
    static std::size_t occ(const graphs::LollipopGraph<CS, PL>& g) { return g.clique_.occupied_count(); }
    static bool bridge_occ(const graphs::LollipopGraph<CS, PL>& g) { return g.bridge_occupied_; }
    static bool bridge_color(const graphs::LollipopGraph<CS, PL>& g) { return g.bridge_color_; }
    static std::size_t clique_unhappy(const graphs::LollipopGraph<CS, PL>& g) { return g.clique_.unhappy_count(); }
    static std::size_t path_unhappy(const graphs::LollipopGraph<CS, PL>& g) { return g.path_.unhappy_count(); }
    static bool bridge_unhappy(const graphs::LollipopGraph<CS, PL>& g) { return g.bridge_unhappy(); }
    static bool bridge_unhappy_clique(const graphs::LollipopGraph<CS, PL>& g) { return g.bridge_unhappy_in_clique_sense_(); }
};

}}

#else

namespace graphs { namespace test {
template<std::size_t CS, std::size_t PL>
struct LollipopAccess; // forward decl when disabled
}}

#endif
