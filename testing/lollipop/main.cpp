// Ref vs Impl: Exhaustive unhappy_count comparison on a tiny lollipop (CS=3, PL=2)

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstddef>
#include <utility>
#include <optional>
#include <optional>
#include "core/schelling_threshold.hpp"
#include "graphs/lollipop.hpp"
#include "graphs/testing/lollipop_access.hpp"

namespace {
using LG = graphs::LollipopGraph<3, 2>;
using LA = graphs::test::LollipopAccess<3, 2>;

static inline void set_tau_force(int p, int q) {
    core::schelling::program_threshold.p = static_cast<core::color_count_t>(p);
    core::schelling::program_threshold.q = static_cast<core::color_count_t>(q);
    core::schelling::minority_happy_ = !core::schelling::program_threshold.is_unhappy(1, 2);
}

template <std::size_t CS, std::size_t PL>
struct RefLollipop {
    std::size_t c0{0}, c1{0};
    bool bridge_occ{false};
    bool bridge_col{false};
    bool p_occ[PL]{};
    bool p_col[PL]{};
    static constexpr std::size_t CliqueBase = 0;
    static constexpr std::size_t PathBase   = CS;

    void clear() {
        c0 = c1 = 0; bridge_occ = false; bridge_col = false;
        for (std::size_t j = 0; j < PL; ++j) { p_occ[j] = false; p_col[j] = false; }
    }
    void set_bridge(int tri) {
        if (tri == 0) { bridge_occ = false; bridge_col = false; }
        else { bridge_occ = true; bridge_col = (tri == 2); if (bridge_col) ++c1; else ++c0; }
    }
    void add_clique(int tri) { if (tri == 1) ++c0; else if (tri == 2) ++c1; }
    void set_path(std::size_t j, int tri) { if (tri == 0) { p_occ[j] = false; p_col[j] = false; } else { p_occ[j] = true; p_col[j] = (tri == 2); } }

    // Counts-first bridge index (mirrors LollipopGraph::bridge_index_)
    std::size_t bridge_index() const noexcept { return bridge_occ ? (bridge_col ? c0 : 0u) : (c0 + c1); }

    // Pop agent at global index; returns color (false=0, true=1)
    bool pop_agent(std::size_t from) noexcept {
        if (from < CS) {
            const std::size_t bi = bridge_index();
            if (bridge_occ && from == bi) {
                const bool col = bridge_col;
                if (col) --c1; else --c0;
                bridge_occ = false; bridge_col = false;
                return col;
            } else {
                const bool col = (from >= c0);
                if (col) --c1; else --c0;
                return col;
            }
        } else {
            const std::size_t j = from - PathBase;
            const bool col = p_col[j];
            p_occ[j] = false; p_col[j] = false;
            return col;
        }
    }

    // Place agent at global index with color c (false=0, true=1)
    void place_agent(std::size_t to, bool c) noexcept {
        if (to == bridge_index()) {
            bridge_occ = true; bridge_col = c;
            if (c) ++c1; else ++c0;
        } else if (to < CS) {
            if (c) ++c1; else ++c0;
        } else {
            const std::size_t j = to - PathBase;
            p_occ[j] = true; p_col[j] = c;
        }
    }

    std::size_t unhappy_count() const {
        const std::size_t occ = c0 + c1;
        std::size_t total = 0;
        if (occ > 0) {
            const std::size_t neigh = occ - 1;
            const bool u0 = core::schelling::is_unhappy(c1, neigh);
            const bool u1 = core::schelling::is_unhappy(c0, neigh);
            const std::size_t nb0 = c0 - (bridge_occ && !bridge_col ? 1u : 0u);
            const std::size_t nb1 = c1 - (bridge_occ && bridge_col ? 1u : 0u);
            if (u0) total += nb0;
            if (u1) total += nb1;
        }
        if (bridge_occ) {
            const std::size_t neigh_b = (occ - 1) + (p_occ[0] ? 1u : 0u);
            const std::size_t disagree_b = (bridge_col ? c0 : c1) + ((p_occ[0] && (p_col[0] != bridge_col)) ? 1u : 0u);
            if (core::schelling::is_unhappy(disagree_b, neigh_b)) ++total;
        }
        for (std::size_t j = 0; j < PL; ++j) {
            if (!p_occ[j]) continue;
            const bool col = p_col[j];
            std::size_t neigh = 0, disagree = 0;
            if (j == 0) { if (bridge_occ) { ++neigh; disagree += (bridge_col != col); } }
            else { if (p_occ[j-1]) { ++neigh; disagree += (p_col[j-1] != col); } }
            if (j + 1 < PL && p_occ[j+1]) { ++neigh; disagree += (p_col[j+1] != col); }
            if (core::schelling::is_unhappy(disagree, neigh)) ++total;
        }
        return total;
    }
};

static inline std::size_t safe_non_bridge_index(const LG& g) {
    const std::size_t bi = LA::bridge_index(g);
    return (bi + 1) % 3; // CS=3 in this test
}

// Generic helpers for scaled tests (templated on sizes)
template <std::size_t CS, std::size_t PL>
static inline std::size_t safe_non_bridge_index(const graphs::LollipopGraph<CS, PL>& g) {
    using AX = graphs::test::LollipopAccess<CS, PL>;
    const std::size_t bi = AX::bridge_index(g);
    return (bi + 1) % CS;
}

template <std::size_t CS, std::size_t PL>
static inline std::optional<std::size_t> idx_pop_nonbridge_c0(const graphs::LollipopGraph<CS, PL>& g) {
    using AX = graphs::test::LollipopAccess<CS, PL>;
    const std::size_t c0 = AX::c0(g);
    const bool bocc = AX::bridge_occ(g);
    const bool bcol = AX::bridge_color(g);
    const std::size_t nonbridge = c0 - (bocc && !bcol ? 1u : 0u);
    if (nonbridge == 0) return std::nullopt;
    return (bocc && !bcol) ? std::optional<std::size_t>(1u) : std::optional<std::size_t>(0u);
}

template <std::size_t CS, std::size_t PL>
static inline std::optional<std::size_t> idx_pop_nonbridge_c1(const graphs::LollipopGraph<CS, PL>& g) {
    using AX = graphs::test::LollipopAccess<CS, PL>;
    const std::size_t c0 = AX::c0(g);
    const std::size_t c1 = AX::c1(g);
    const bool bocc = AX::bridge_occ(g);
    const bool bcol = AX::bridge_color(g);
    const std::size_t nonbridge = c1 - (bocc && bcol ? 1u : 0u);
    if (nonbridge == 0) return std::nullopt;
    return (bocc && bcol) ? std::optional<std::size_t>(c0 + 1u) : std::optional<std::size_t>(c0);
}

template <std::size_t CS, std::size_t PL>
static inline std::size_t idx_place_nonbridge_c0(const graphs::LollipopGraph<CS, PL>& g) {
    using AX = graphs::test::LollipopAccess<CS, PL>;
    const bool bocc = AX::bridge_occ(g);
    const bool bcol = AX::bridge_color(g);
    return (bocc && !bcol) ? 1u : 0u;
}

template <std::size_t CS, std::size_t PL>
static inline std::size_t idx_place_nonbridge_c1(const graphs::LollipopGraph<CS, PL>& g) {
    using AX = graphs::test::LollipopAccess<CS, PL>;
    const std::size_t c0 = AX::c0(g);
    const bool bocc = AX::bridge_occ(g);
    const bool bcol = AX::bridge_color(g);
    return (bocc && bcol) ? (c0 + 1u) : c0;
}

} // namespace

TEST_CASE("RefLollipop vs LollipopGraph: unhappy_count equality (CS=3, PL=2) across tau and states") {
    using Ref = RefLollipop<3, 2>;
    const std::pair<int,int> taus[] = { {0,1}, {1,3}, {1,2}, {2,3}, {1,1} };
    for (auto [p, q] : taus) {
        set_tau_force(p, q);
        for (int b = 0; b < 3; ++b) {
            for (int cA = 0; cA < 3; ++cA) {
                for (int cB = 0; cB < 3; ++cB) {
                    for (int p0 = 0; p0 < 3; ++p0) {
                        for (int p1 = 0; p1 < 3; ++p1) {
                            Ref ref; ref.clear();
                            ref.set_bridge(b);
                            ref.add_clique(cA);
                            ref.add_clique(cB);
                            ref.set_path(0, p0);
                            ref.set_path(1, p1);

                            LG g;
                            if (b) g.place_agent(LA::bridge_index(g), b == 2);
                            if (cA) g.place_agent(safe_non_bridge_index(g), cA == 2);
                            if (cB) g.place_agent(safe_non_bridge_index(g), cB == 2);
                            if (p0) g.place_agent(LG::PathBase + 0, p0 == 2);
                            if (p1) g.place_agent(LG::PathBase + 1, p1 == 2);

                            CHECK(ref.unhappy_count() == g.unhappy_count());
                        }
                    }
                }
            }
        }
    }
}

// Helpers to drive clique ops by color semantics (avoid accidental bridge toggles)
static inline std::optional<std::size_t> idx_pop_nonbridge_c0(const LG& g) {
    const std::size_t c0 = LA::c0(g);
    const bool bocc = LA::bridge_occ(g);
    const bool bcol = LA::bridge_color(g);
    const std::size_t nonbridge = c0 - (bocc && !bcol ? 1u : 0u);
    if (nonbridge == 0) return std::nullopt;
    return (bocc && !bcol) ? std::optional<std::size_t>(1u) : std::optional<std::size_t>(0u);
}
static inline std::optional<std::size_t> idx_pop_nonbridge_c1(const LG& g) {
    const std::size_t c0 = LA::c0(g);
    const std::size_t c1 = LA::c1(g);
    const bool bocc = LA::bridge_occ(g);
    const bool bcol = LA::bridge_color(g);
    const std::size_t nonbridge = c1 - (bocc && bcol ? 1u : 0u);
    if (nonbridge == 0) return std::nullopt;
    return (bocc && bcol) ? std::optional<std::size_t>(c0 + 1u) : std::optional<std::size_t>(c0);
}
static inline std::size_t idx_place_nonbridge_c0(const LG& g) {
    const bool bocc = LA::bridge_occ(g);
    const bool bcol = LA::bridge_color(g);
    return (bocc && !bcol) ? 1u : 0u;
}
static inline std::size_t idx_place_nonbridge_c1(const LG& g) {
    const std::size_t c0 = LA::c0(g);
    const bool bocc = LA::bridge_occ(g);
    const bool bcol = LA::bridge_color(g);
    return (bocc && bcol) ? (c0 + 1u) : c0;
}

template<typename R>
static inline std::optional<std::size_t> ref_idx_pop_nonbridge_c0(const R& r) {
    const std::size_t nonbridge = r.c0 - (r.bridge_occ && !r.bridge_col ? 1u : 0u);
    if (nonbridge == 0) return std::nullopt;
    return (r.bridge_occ && !r.bridge_col) ? std::optional<std::size_t>(1u) : std::optional<std::size_t>(0u);
}
template<typename R>
static inline std::optional<std::size_t> ref_idx_pop_nonbridge_c1(const R& r) {
    const std::size_t nonbridge = r.c1 - (r.bridge_occ && r.bridge_col ? 1u : 0u);
    if (nonbridge == 0) return std::nullopt;
    return (r.bridge_occ && r.bridge_col) ? std::optional<std::size_t>(r.c0 + 1u) : std::optional<std::size_t>(r.c0);
}
template<typename R>
static inline std::size_t ref_idx_place_nonbridge_c0(const R& r) {
    return (r.bridge_occ && !r.bridge_col) ? 1u : 0u;
}
template<typename R>
static inline std::size_t ref_idx_place_nonbridge_c1(const R& r) {
    return (r.bridge_occ && r.bridge_col) ? (r.c0 + 1u) : r.c0;
}

TEST_CASE("Ref vs Impl: clique semantic pops + restore preserve unhappy_count") {
    using Ref = RefLollipop<3, 2>;
    const std::pair<int,int> taus[] = { {0,1}, {1,3}, {1,2}, {2,3}, {1,1} };
    for (auto [p, q] : taus) {
        set_tau_force(p, q);
        for (int b = 0; b < 3; ++b) {
            for (int cA = 0; cA < 3; ++cA) {
                for (int cB = 0; cB < 3; ++cB) {
                    for (int p0 = 0; p0 < 3; ++p0) {
                        for (int p1 = 0; p1 < 3; ++p1) {
                            Ref ref; ref.clear();
                            ref.set_bridge(b);
                            ref.add_clique(cA);
                            ref.add_clique(cB);
                            ref.set_path(0, p0);
                            ref.set_path(1, p1);

                            LG g;
                            if (b) g.place_agent(LA::bridge_index(g), b == 2);
                            if (cA) g.place_agent(safe_non_bridge_index(g), cA == 2);
                            if (cB) g.place_agent(safe_non_bridge_index(g), cB == 2);
                            if (p0) g.place_agent(LG::PathBase + 0, p0 == 2);
                            if (p1) g.place_agent(LG::PathBase + 1, p1 == 2);

                            // 1) Bridge pop/restore if present
                            if (LA::bridge_occ(g)) {
                                LG g1 = g; Ref r1 = ref;
                                const std::size_t bi_g = LA::bridge_index(g1);
                                const std::size_t bi_r = r1.bridge_index();
                                const bool c_g = g1.pop_agent(bi_g);
                                const bool c_r = r1.pop_agent(bi_r);
                                (void)c_r;
                                CHECK(r1.unhappy_count() == g1.unhappy_count());
                                g1.place_agent(LA::bridge_index(g1), c_g);
                                r1.place_agent(r1.bridge_index(), c_g);
                                CHECK(r1.unhappy_count() == g1.unhappy_count());
                            }

                            // 2) Non-bridge color0 pop/restore if exists
                            if (auto i0 = idx_pop_nonbridge_c0(g)) {
                                if (auto j0 = ref_idx_pop_nonbridge_c0(ref)) {
                                    LG g1 = g; Ref r1 = ref;
                                    const bool cg = g1.pop_agent(*i0);
                                    const bool cr = r1.pop_agent(*j0);
                                    (void)cg; (void)cr; // both should be color0
                                    CHECK(r1.unhappy_count() == g1.unhappy_count());
                                    g1.place_agent(idx_place_nonbridge_c0(g1), false);
                                    r1.place_agent(ref_idx_place_nonbridge_c0(r1), false);
                                    CHECK(r1.unhappy_count() == g1.unhappy_count());
                                }
                            }

                            // 3) Non-bridge color1 pop/restore if exists
                            if (auto i1 = idx_pop_nonbridge_c1(g)) {
                                if (auto j1 = ref_idx_pop_nonbridge_c1(ref)) {
                                    LG g1 = g; Ref r1 = ref;
                                    const bool cg = g1.pop_agent(*i1);
                                    const bool cr = r1.pop_agent(*j1);
                                    (void)cg; (void)cr; // both should be color1
                                    CHECK(r1.unhappy_count() == g1.unhappy_count());
                                    g1.place_agent(idx_place_nonbridge_c1(g1), true);
                                    r1.place_agent(ref_idx_place_nonbridge_c1(r1), true);
                                    CHECK(r1.unhappy_count() == g1.unhappy_count());
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

TEST_CASE("Ref vs Impl: path pops + restore preserve unhappy_count") {
    using Ref = RefLollipop<3, 2>;
    const std::pair<int,int> taus[] = { {0,1}, {1,3}, {1,2}, {2,3}, {1,1} };
    for (auto [p, q] : taus) {
        set_tau_force(p, q);
        for (int b = 0; b < 3; ++b) {
            for (int cA = 0; cA < 3; ++cA) {
                for (int cB = 0; cB < 3; ++cB) {
                    for (int p0 = 0; p0 < 3; ++p0) {
                        for (int p1 = 0; p1 < 3; ++p1) {
                            Ref ref; ref.clear();
                            ref.set_bridge(b);
                            ref.add_clique(cA);
                            ref.add_clique(cB);
                            ref.set_path(0, p0);
                            ref.set_path(1, p1);

                            LG g;
                            if (b) g.place_agent(LA::bridge_index(g), b == 2);
                            if (cA) g.place_agent(safe_non_bridge_index(g), cA == 2);
                            if (cB) g.place_agent(safe_non_bridge_index(g), cB == 2);
                            if (p0) g.place_agent(LG::PathBase + 0, p0 == 2);
                            if (p1) g.place_agent(LG::PathBase + 1, p1 == 2);

                            // Try popping/restoring each path vertex if occupied
                            for (int j = 0; j < 2; ++j) {
                                const std::size_t gi = LG::PathBase + static_cast<std::size_t>(j);
                                if (!ref.p_occ[j]) continue;
                                LG g1 = g; Ref r1 = ref;
                                const bool c = g1.pop_agent(gi);
                                (void)r1.pop_agent(gi);
                                CHECK(r1.unhappy_count() == g1.unhappy_count());
                                g1.place_agent(gi, c);
                                r1.place_agent(gi, c);
                                CHECK(r1.unhappy_count() == g1.unhappy_count());
                            }
                        }
                    }
                }
            }
        }
    }
}

// --------------------------- Scaled tests ----------------------------------

#ifndef LOLLIPOP_SCALED_STATES
#define LOLLIPOP_SCALED_STATES 256
#endif

#ifndef LOLLIPOP_SCALED_STEPS
#define LOLLIPOP_SCALED_STEPS 512
#endif

TEST_CASE("Scaled random states: unhappy_count parity (CS=13, PL=17)") {
    using LGX = graphs::LollipopGraph<13, 17>;
    using LAX = graphs::test::LollipopAccess<13, 17>;
    using Ref = RefLollipop<13, 17>;
    std::mt19937_64 rng(0xD00DFEEDULL);
    std::uniform_int_distribution<int> tri(0, 2);
    const std::pair<int,int> taus[] = { {0,1}, {1,3}, {1,2}, {2,3}, {1,1} };
    for (auto [p,q] : taus) {
        set_tau_force(p, q);
        for (int it = 0; it < LOLLIPOP_SCALED_STATES; ++it) {
            Ref ref; ref.clear();
            LGX g;
            // Bridge
            const int b = tri(rng);
            if (b) { const auto to = LAX::bridge_index(g); const bool col = (b == 2); g.place_agent(to, col); ref.place_agent(to, col); }
            // Additional clique counts
            const std::size_t slack = 13 - (b ? 1u : 0u);
            std::uniform_int_distribution<std::size_t> occd(0, slack);
            const std::size_t occ_extra = occd(rng);
            std::uniform_int_distribution<std::size_t> c1d(0, occ_extra);
            const std::size_t c1_extra = c1d(rng);
            const std::size_t c0_extra = occ_extra - c1_extra;
            for (std::size_t k = 0; k < c0_extra; ++k) { auto to = idx_place_nonbridge_c0<13,17>(g); g.place_agent(to, false); ref.place_agent(to, false); }
            for (std::size_t k = 0; k < c1_extra; ++k) { auto to = idx_place_nonbridge_c1<13,17>(g); g.place_agent(to, true);  ref.place_agent(to, true);  }
            // Path
            for (std::size_t j = 0; j < 17; ++j) {
                const int t = tri(rng);
                if (t) { const auto to = LGX::PathBase + j; const bool col = (t == 2); g.place_agent(to, col); ref.place_agent(to, col); }
            }
            CHECK(ref.unhappy_count() == g.unhappy_count());
        }
    }
}

TEST_CASE("Scaled random semantic pops/restores preserve unhappy_count (CS=13, PL=17)") {
    using LGX = graphs::LollipopGraph<13, 17>;
    using LAX = graphs::test::LollipopAccess<13, 17>;
    using Ref = RefLollipop<13, 17>;
    std::mt19937_64 rng(0xBADC0FFEEULL);
    std::uniform_int_distribution<int> tri(0, 2);
    std::bernoulli_distribution choose(0.5);
    const std::pair<int,int> taus[] = { {0,1}, {1,3}, {1,2}, {2,3}, {1,1} };
    for (auto [p,q] : taus) {
        set_tau_force(p, q);
        for (int it = 0; it < LOLLIPOP_SCALED_STATES; ++it) {
            Ref ref; ref.clear();
            LGX g;
            const int b = tri(rng);
            if (b) { const auto to = LAX::bridge_index(g); const bool col = (b == 2); g.place_agent(to, col); ref.place_agent(to, col); }
            const std::size_t slack = 13 - (b ? 1u : 0u);
            std::uniform_int_distribution<std::size_t> occd(0, slack);
            const std::size_t occ_extra = occd(rng);
            std::uniform_int_distribution<std::size_t> c1d(0, occ_extra);
            const std::size_t c1_extra = c1d(rng);
            const std::size_t c0_extra = occ_extra - c1_extra;
            for (std::size_t k = 0; k < c0_extra; ++k) { auto to = idx_place_nonbridge_c0<13,17>(g); g.place_agent(to, false); ref.place_agent(to, false); }
            for (std::size_t k = 0; k < c1_extra; ++k) { auto to = idx_place_nonbridge_c1<13,17>(g); g.place_agent(to, true);  ref.place_agent(to, true);  }
            for (std::size_t j = 0; j < 17; ++j) { const int t = tri(rng); if (t) { auto to = LGX::PathBase + j; bool col = (t == 2); g.place_agent(to,col); ref.place_agent(to,col); } }

            CHECK(ref.unhappy_count() == g.unhappy_count());

            for (int step = 0; step < LOLLIPOP_SCALED_STEPS; ++step) {
                // Build candidate ops
                bool did = false;
                // Bridge pop/restore
                if (LAX::bridge_occ(g) && choose(rng)) {
                    LGX g1 = g; Ref r1 = ref;
                    const std::size_t bi_g = LAX::bridge_index(g1);
                    const std::size_t bi_r = r1.bridge_index();
                    const bool c_g = g1.pop_agent(bi_g); (void)r1.pop_agent(bi_r);
                    CHECK(r1.unhappy_count() == g1.unhappy_count());
                    g1.place_agent(LAX::bridge_index(g1), c_g);
                    r1.place_agent(r1.bridge_index(), c_g);
                    CHECK(r1.unhappy_count() == g1.unhappy_count());
                    g = std::move(g1); ref = std::move(r1); did = true;
                }
                // Non-bridge color0
                if (!did) if (auto i0 = idx_pop_nonbridge_c0<13,17>(g)) {
                    LGX g1 = g; Ref r1 = ref;
                    (void)g1.pop_agent(*i0);
                    (void)r1.pop_agent(ref_idx_pop_nonbridge_c0(ref).value());
                    CHECK(r1.unhappy_count() == g1.unhappy_count());
                    g1.place_agent(idx_place_nonbridge_c0<13,17>(g1), false);
                    r1.place_agent(ref_idx_place_nonbridge_c0(r1), false);
                    CHECK(r1.unhappy_count() == g1.unhappy_count());
                    g = std::move(g1); ref = std::move(r1); did = true;
                }
                // Non-bridge color1
                if (!did) if (auto i1 = idx_pop_nonbridge_c1<13,17>(g)) {
                    LGX g1 = g; Ref r1 = ref;
                    (void)g1.pop_agent(*i1);
                    (void)r1.pop_agent(ref_idx_pop_nonbridge_c1(ref).value());
                    CHECK(r1.unhappy_count() == g1.unhappy_count());
                    g1.place_agent(idx_place_nonbridge_c1<13,17>(g1), true);
                    r1.place_agent(ref_idx_place_nonbridge_c1(r1), true);
                    CHECK(r1.unhappy_count() == g1.unhappy_count());
                    g = std::move(g1); ref = std::move(r1); did = true;
                }
                // Path pop if any
                if (!did) {
                    // Find an occupied path index (if any)
                    std::vector<std::size_t> occs; occs.reserve(17);
                    for (std::size_t j = 0; j < 17; ++j) if (ref.p_occ[j]) occs.push_back(j);
                    if (!occs.empty()) {
                        std::uniform_int_distribution<std::size_t> pick(0, occs.size()-1);
                        const std::size_t j = occs[pick(rng)];
                        LGX g1 = g; Ref r1 = ref;
                        const std::size_t gi = LGX::PathBase + j;
                        const bool c = g1.pop_agent(gi);
                        (void)r1.pop_agent(gi);
                        CHECK(r1.unhappy_count() == g1.unhappy_count());
                        g1.place_agent(gi, c);
                        r1.place_agent(gi, c);
                        CHECK(r1.unhappy_count() == g1.unhappy_count());
                        g = std::move(g1); ref = std::move(r1); did = true;
                    }
                }
                if (!did) break; // nothing to do
            }
        }
    }
}
