// clique_tests.cpp
// Focused doctest-based checks for graphs::Clique using counts-only invariants.
//
// Build example:
//   make -C testing/clique
//   make -C testing/clique run

#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <utility>

#include "core/bitset.hpp"
#include "core/schelling_threshold.hpp"
#include "graphs/clique.hpp"

#ifndef CLIQUE_TEST_STRESS_SCALE
#define CLIQUE_TEST_STRESS_SCALE 4096
#endif

namespace testutil {

struct ThresholdInit {
    ThresholdInit() { core::schelling::init_program_threshold(1, 2); }
} _threshold_init;

inline void set_tau(int p, int q) noexcept {
    core::schelling::init_program_threshold(static_cast<core::color_count_t>(p),
                                            static_cast<core::color_count_t>(q));
}

inline std::mt19937_64 make_rng(std::uint64_t seed = 0xC11C0FFEEULL) {
    return std::mt19937_64{seed};
}

template <std::size_t Size>
core::bitset<Size> make_occupied_mask(std::size_t occ_count) {
    core::bitset<Size> mask;
    mask.reset();
    for (std::size_t i = 0; i < occ_count; ++i) {
        mask.set(i);
    }
    return mask;
}

template <std::size_t Size>
core::bitset<Size> make_color_mask(std::size_t c0, std::size_t c1) {
    core::bitset<Size> mask;
    mask.reset();
    for (std::size_t i = 0; i < c1; ++i) {
        mask.set(c0 + i);
    }
    return mask;
}

template <std::size_t Size>
graphs::Clique<Size> make_clique(std::size_t c0, std::size_t c1) {
    const std::size_t occ = c0 + c1;
    auto occ_mask = make_occupied_mask<Size>(occ);
    auto color_mask = make_color_mask<Size>(c0, c1);
    return graphs::Clique<Size>(occ_mask, color_mask);
}

template <std::size_t Size>
void check_clique_state(const graphs::Clique<Size>& clique,
                        std::size_t                 c0,
                        std::size_t                 c1) {
    const std::size_t occ = c0 + c1;
    const std::size_t unocc = Size - occ;

    CHECK(clique.count_by_color(std::nullopt) == unocc);
    CHECK(clique.count_by_color(0) == c0);
    CHECK(clique.count_by_color(1) == c1);

    for (std::size_t i = 0; i < occ; ++i) {
        const bool expected_color = (i >= c0);
        CHECK(clique.get_color(i) == expected_color);
    }

    const std::size_t neighbors = occ-1;
    CAPTURE(neighbors);
    CAPTURE(c0);
    CAPTURE(c1);

    std::size_t expected_unhappy = static_cast<std::size_t>(core::schelling::is_unhappy(c1, neighbors))*c0
                                    + static_cast<std::size_t>(core::schelling::is_unhappy(c0, neighbors))*c1;

    CHECK(clique.unhappy_count() == expected_unhappy);
}

template <std::size_t Size>
void sweep_all_states() {
    for (std::size_t occ = 0; occ <= Size; ++occ) {
        for (std::size_t c1 = 0; c1 <= occ; ++c1) {
            const std::size_t c0 = occ - c1;
            CAPTURE(Size);
            CAPTURE(occ);
            CAPTURE(c1);
            auto clique = make_clique<Size>(c0, c1);
            check_clique_state(clique, c0, c1);
        }
    }
}

} // namespace testutil

// -----------------------------------------------------------------------------
// Exhaustive constructor checks for small clique sizes
// -----------------------------------------------------------------------------

TEST_CASE("Clique counts mirror constructor bitsets for all states: Size=5") {
    constexpr std::size_t Size = 5;
    for (std::size_t occ = 0; occ <= Size; ++occ) {
        for (std::size_t c1 = 0; c1 <= occ; ++c1) {
            const std::size_t c0 = occ - c1;
            CAPTURE(occ);
            CAPTURE(c1);
            auto clique = testutil::make_clique<Size>(c0, c1);
            testutil::check_clique_state(clique, c0, c1);
        }
    }
}

// -----------------------------------------------------------------------------
// τ sweeps for unhappy-count formulas across multiple clique sizes
// -----------------------------------------------------------------------------

TEST_CASE("Clique unhappy counts align with τ sweeps: Size=4,7,9") {
    const std::pair<int,int> taus[] = {
        {1,3}, {1,2}, {2,3}
    };
    for (auto [p,q] : taus) {
        CAPTURE(p);
        CAPTURE(q);
        testutil::set_tau(p, q);
        testutil::sweep_all_states<4>();
        testutil::sweep_all_states<7>();
        testutil::sweep_all_states<9>();
    }
}

// -----------------------------------------------------------------------------
// Pop/place stress with random operations across τ values
// -----------------------------------------------------------------------------

TEST_CASE("Clique pop/place stress preserves counts across τ values: Size=61") {
    const std::pair<int,int> taus[] = {
        {1,3}, {1,2}, {2,3}
    };
    std::mt19937_64 rng = testutil::make_rng(0xC11C0FFEEULL);
    std::bernoulli_distribution choose_pop(0.5);
    std::bernoulli_distribution color_choice(0.5);

    constexpr std::size_t Size = 101;
    const std::size_t iterations = CLIQUE_TEST_STRESS_SCALE / 32 + Size;

    for (auto [p,q] : taus) {
        CAPTURE(p);
        CAPTURE(q);
        testutil::set_tau(p, q);

        for (std::size_t trial = 0; trial < 32; ++trial) {
            std::uniform_int_distribution<std::size_t> occ_dist(0, Size);
            const std::size_t occ = occ_dist(rng);
            std::uniform_int_distribution<std::size_t> c1_dist(0, occ);
            std::size_t c1 = c1_dist(rng);
            CHECK(c1 <= occ);
            std::size_t c0 = occ - c1;
            
            auto clique = testutil::make_clique<Size>(c0, c1);
            testutil::check_clique_state(clique, c0, c1);

            for (std::size_t it = 0; it < iterations; ++it) {
                const std::size_t occ_now = c0 + c1;
                const std::size_t unocc_now = Size - occ_now;

                CHECK(occ_now == clique.occupied_count());
                CHECK(unocc_now == clique.count_by_color(std::nullopt));
                CHECK(c0 == clique.count_by_color(0));
                CHECK(c1 == clique.count_by_color(1));

                bool do_pop = choose_pop(rng);
                if (occ_now == 0) do_pop = false;
                if (unocc_now == 0) do_pop = true;

                if (do_pop) {
                    bool color = clique.get_unhappy(rng).value();
                    auto current_count = clique.count_by_color(color);
                    auto popped = clique.pop_agent(c0*color); // 0 if color==0 else c0

                    CAPTURE(color);
                    CAPTURE(popped);
                    CAPTURE(current_count);
                    CAPTURE(c0);
                    CAPTURE(c1);

                    auto new_count = clique.count_by_color(color);
                    CHECK(new_count == current_count-1);

                    CHECK(popped.has_value());
                    CHECK(popped.value() == color);
                    if (color) {
                        --c1;
                    } else {
                        --c0;
                    }
                } else {
                    bool color = color_choice(rng);
                    clique.place_agent(0, color);
                    if (color) {
                        ++c1;
                    } else {
                        ++c0;
                    }
                }

                testutil::check_clique_state(clique, c0, c1);
            }
        }
    }
}

int main(int argc, char** argv) {
    doctest::Context ctx;
    ctx.setOption("abort-after", 5); // stop test execution after 5 failed asserts
    ctx.applyCommandLine(argc, argv);
    int res = ctx.run(); // Run tests unless --no-run is specified
    if (ctx.shouldExit()) { // Propagate the result of the tests
        return res;
    }
    return res; // The result from doctest is propagated here as well
}