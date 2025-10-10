// path_tests.cpp
// Minimal, readable tests for graphs/Path<B> using doctest.
//
// Build (example):
//   g++ -std=c++20 -O2 -I./tests/third_party -I. path_tests.cpp -o path_tests
//   ./path_tests
//
// Vendoring doctest: https://github.com/doctest/doctest (single header)
//
// Adjust stress iterations (defaults to 4096 * B) by defining PATH_TEST_STRESS_SCALE:
//   g++ ... -DPATH_TEST_STRESS_SCALE=16384  # ~original intensity

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>
#include <optional>
#include <random>

// Your project headers
#include "core/bitset.hpp"
#include "core/schelling_threshold.hpp"
#include "graphs/path.hpp"
#include "graphs/testing/path_access.hpp"

#ifndef PATH_TEST_STRESS_SCALE
#define PATH_TEST_STRESS_SCALE 16384
#endif

namespace testutil {

// Ensure Schelling thresholds are set before tests run.
struct ThresholdInit {
    ThresholdInit() { core::schelling::init_program_threshold(1, 2); }
} _threshold_init;

inline void set_tau(int p, int q) noexcept {
    // Re-init program-wide threshold for tests (keeps internal caches consistent)
    core::schelling::init_program_threshold(p, q);
}

// Deterministic RNG
inline std::mt19937_64 make_rng(std::uint64_t seed = 0xC0FFEE123456789ULL) {
    return std::mt19937_64{seed};
}

// Invariant checks shared by all tests:
//   1) View matches constructor inputs.
//   2) Counts by color match expectations.
//   3) Unhappiness flags and unhappy_count() match recomputation.
template <std::size_t B>
void check_path_consistency(const core::bitset<B>& unocc,
                            const core::bitset<B>& colors,
                            const Path<B>&         path) {
    // (1) Verify raw view -> inputs
    core::bitset<B> got_unocc, got_c1;
    got_unocc.reset();
    got_c1.reset();

    for (std::size_t i = 0; i < B; ++i) {
        if (path.is_unoccupied(i)) {
            got_unocc.set(i);
        } else if (path.get_color(i)) {
            got_c1.set(i);
        }
    }

    CHECK(got_unocc == unocc);
    CHECK(got_c1    == colors);

    // (2) Counts
    std::size_t expected_gap = 0, expected_c1 = 0;
    for (std::size_t i = 0; i < B; ++i) {
        if (unocc.test(i))       ++expected_gap;
        else if (colors.test(i)) ++expected_c1;
    }
    const std::size_t expected_c0 = B - expected_gap - expected_c1;

    CHECK(path.count_by_color(std::nullopt) == expected_gap);
    CHECK(path.count_by_color(true)         == expected_c1);
    CHECK(path.count_by_color(false)        == expected_c0);

    // (3) Unhappiness
    std::size_t expected_unhappy = 0;
    for (std::size_t i = 0; i < B; ++i) {
        if (unocc.test(i)) {
            CHECK(path.is_unhappy(i) == false);
            continue;
        }

        const bool color = colors.test(i);
        std::uint32_t neighbors = 0, frustration = 0;

        if (i > 0 && !unocc.test(i - 1)) {
            ++neighbors; frustration += (colors.test(i - 1) != color);
        }
        if (i + 1 < B && !unocc.test(i + 1)) {
            ++neighbors; frustration += (colors.test(i + 1) != color);
        }

        const bool unhappy = core::schelling::is_unhappy(frustration, neighbors);
        CHECK(path.is_unhappy(i) == unhappy);
        expected_unhappy += unhappy ? 1u : 0u;
    }

    CHECK(path.unhappy_count() == expected_unhappy);
}

// Exhaustively enumerate all tri-states for small B (3^B):
// digit mapping used here (simple and explicit):
//   0 -> unoccupied, 1 -> occupied color 0, 2 -> occupied color 1
template <std::size_t B>
void exhaust_all_states_small_B() {
    std::size_t total = 1;
    for (std::size_t i = 0; i < B; ++i) total *= 3;

    for (std::size_t t = 0; t < total; ++t) {
        core::bitset<B> unocc, colors;
        unocc.reset(); colors.reset();

        std::size_t x = t;
        for (std::size_t i = 0; i < B; ++i) {
            const std::size_t d = x % 3; // base-3 digit for site i
            x /= 3;
            if (d == 0) {
                unocc.set(i);           // unoccupied
            } else if (d == 2) {
                colors.set(i);          // occupied color 1
            } // d == 1 -> occupied color 0 (do nothing)
        }

        Path<B> path(unocc, colors);
        check_path_consistency(unocc, colors, path);
    }
}

// Randomize a valid state: {unoccupied, color0, color1}
template <std::size_t B, class Rng>
void randomize_state(core::bitset<B>& unocc,
                     core::bitset<B>& colors,
                     Rng&             rng) {
    std::uniform_int_distribution<int> d(0, 2);
    unocc.reset(); colors.reset();
    for (std::size_t i = 0; i < B; ++i) {
        switch (d(rng)) {
            case 0: /* occupied color 0 */ break;
            case 1: colors.set(i); break;    // occupied color 1
            case 2: unocc.set(i);  break;    // unoccupied
        }
    }
}

// Stress pop/place at random positions and verify invariants each step.
template <std::size_t B>
void pop_place_stress(std::mt19937_64& rng,
                      std::size_t iterations = PATH_TEST_STRESS_SCALE * B) {
    static_assert(B % 2 == 1, "B should be odd for this stress test.");

    core::bitset<B> unocc, colors;
    randomize_state(unocc, colors, rng);
    Path<B> path(unocc, colors);

    std::bernoulli_distribution color_choice(0.5);
    std::bernoulli_distribution choose_pop(0.5);

    for (std::size_t it = 0; it < iterations; ++it) {
        const std::size_t unocc_count = unocc.count();
        bool do_pop = choose_pop(rng);
        const bool can_place = unocc_count > 0;
        const bool can_pop = path.unhappy_count() > 0;
        if(!can_place && !can_pop) {break;}
                
        if (!can_pop) do_pop = false;
        if (!can_place) do_pop = true;

        CAPTURE(do_pop);

        if(do_pop) {
            CAPTURE(graphs::test::raw_unhappy_cache(path));
            CAPTURE(path.unhappy_count());
            std::size_t idx = path.get_unhappy(rng);
            CAPTURE(idx);
            REQUIRE(idx < B);
            CHECK(path.is_unhappy(idx) == true);
            CHECK(colors.test(idx) == path.pop_agent(idx));
            unocc.set(idx);
            colors.reset(idx);
        } else {
            CAPTURE(graphs::test::raw_occ(path));
            CAPTURE(unocc_count);
            std::size_t idx = path.get_unoccupied(rng);
            CAPTURE(idx);
            REQUIRE(idx < B);
            CHECK(path.is_unoccupied(idx) == true);
            bool c = color_choice(rng);
            path.place_agent(idx, c);
            CHECK(path.get_color(idx) == c);
            unocc.reset(idx);
            if (c) colors.set(idx); else colors.reset(idx);
        }

        // // Check all invariants every step.
        check_path_consistency(unocc, colors, path);
    }
}

} // namespace testutil

// -----------------------------------------------------------------------------
// Test cases
// -----------------------------------------------------------------------------

TEST_CASE("Constructor invariants hold for all states: B=2") {
    testutil::exhaust_all_states_small_B<2>();
}

TEST_CASE("Constructor invariants hold for all states: B=4") {
    testutil::exhaust_all_states_small_B<4>();
}

TEST_CASE("Constructor invariants hold for all states: B=8") {
    testutil::exhaust_all_states_small_B<8>();
}

TEST_CASE("Constructor invariants hold for all states: B=10000") {
    auto rng = testutil::make_rng();
    const std::size_t iterations = PATH_TEST_STRESS_SCALE; // scaled by macro
    for (std::size_t it = 0; it < iterations; ++it) {
        core::bitset<10000> unocc, colors;
        testutil::randomize_state(unocc, colors, rng);
        Path<10000> path(unocc, colors);
        testutil::check_path_consistency(unocc, colors, path);
    }
}

TEST_CASE("Pop/place stress preserves invariants: B=61") {
    auto rng = testutil::make_rng();
    testutil::pop_place_stress<61>(rng);
}

TEST_CASE("Preview matches mutation and deltas: B=61") {
    using Bbits = core::bitset<61>;
    std::mt19937_64 rng = testutil::make_rng(0xA11CE5EEDULL);

    Bbits unocc, colors;
    testutil::randomize_state(unocc, colors, rng);
    Path<61> path(unocc, colors);

    std::uniform_int_distribution<std::size_t> pos(0, 60);
    std::bernoulli_distribution color_choice(0.5);

    const std::size_t iterations = PATH_TEST_STRESS_SCALE; // scaled by macro
    for (std::size_t it = 0; it < iterations; ++it) {
        const std::size_t idx = pos(rng);
        const std::size_t raw_idx = idx + 2;

        // Snapshot raw state before mutation
        auto before_occ_raw     = graphs::test::raw_occ(path);
        auto before_colors_raw  = graphs::test::raw_colors(path);

        // Build expected new state & preview masks
        bool placed = false;
        bool new_color = false;
        bool expected_popped_color = false;

        Bbits expected_unocc = unocc;
        Bbits expected_colors = colors;

        if (unocc.test(idx)) {
            placed = true;
            new_color = color_choice(rng);
            expected_unocc.reset(idx);
            if (new_color) expected_colors.set(idx); else expected_colors.reset(idx);
        } else {
            expected_popped_color = colors.test(idx);
            expected_unocc.set(idx);
            expected_colors.reset(idx);
        }

        Path<61> preview(expected_unocc, expected_colors);
        auto proposed_occ_raw     = graphs::test::raw_occ(preview);
        auto proposed_colors_raw  = graphs::test::raw_colors(preview);
        auto proposed_unhappy_raw = graphs::test::raw_unhappy_cache(preview);

        // Apply actual mutation
        if (placed) {
            path.place_agent(idx, new_color);
        } else {
            const bool popped_color = path.pop_agent(idx);
            CHECK_MESSAGE(popped_color == expected_popped_color,
                          "Unexpected pop color at idx= " << idx);
        }

        // Update mirrors
        unocc = expected_unocc;
        colors = expected_colors;

        // Snapshot raw mutated
        auto mutated_occ_raw     = graphs::test::raw_occ(path);
        auto mutated_colors_raw  = graphs::test::raw_colors(path);
        auto mutated_unhappy_raw = graphs::test::raw_unhappy_cache(path);

        // Single-bit deltas at raw_idx
        auto occ_delta   = before_occ_raw;    occ_delta   ^= proposed_occ_raw;
        auto color_delta = before_colors_raw; color_delta ^= proposed_colors_raw;
        const std::size_t occ_ones   = occ_delta.count();
        const std::size_t color_ones = color_delta.count();
        const bool occ_ok   = (occ_delta.none()   || (occ_ones   == 1 && occ_delta.test(raw_idx)));
        const bool color_ok = (color_delta.none() || (color_ones == 1 && color_delta.test(raw_idx)));
        CHECK_MESSAGE(occ_ok,   "occ delta not single-bit at raw_idx; idx= " << idx);
        CHECK_MESSAGE(color_ok, "color delta not single-bit at raw_idx; idx= " << idx);

        // Preview vs mutated should match exactly
        CHECK_MESSAGE(proposed_occ_raw == mutated_occ_raw,
                      "occ raw mismatch after mutation at idx= " << idx);
        CHECK_MESSAGE(proposed_colors_raw == mutated_colors_raw,
                      "colors raw mismatch after mutation at idx= " << idx);
        CHECK_MESSAGE(proposed_unhappy_raw == mutated_unhappy_raw,
                      "unhappy raw mismatch after mutation at idx= " << idx);

        // Also maintain the general invariants
        testutil::check_path_consistency(unocc, colors, path);
    }
}

// -----------------------------------------------------------------------------
// Cross-τ tests
// -----------------------------------------------------------------------------

TEST_CASE("Constructor invariants hold for multiple τ values: B=2,4,8") {
    // Cover τ < 1/2, τ = 1/2, τ > 1/2
    const std::pair<int,int> taus[] = {
        {0,1}, {1,3}, {1,2}, {2,3}, {1,1}
    };
    for (auto [p,q] : taus) {
        CAPTURE(p);
        CAPTURE(q);
        testutil::set_tau(p, q);
        testutil::exhaust_all_states_small_B<2>();
        testutil::exhaust_all_states_small_B<4>();
        testutil::exhaust_all_states_small_B<8>(); 
        // testutil::exhaust_all_states_small_B<16>();
    }
}

TEST_CASE("Pop/place stress across multiple τ values: B=61") {
    const std::pair<int,int> taus[] = {
        {0,1}, {1,3}, {1,2}, {2,3}, {1,1}
    };
    std::mt19937_64 rng = testutil::make_rng(0xBADC0FFEEULL);
    for (auto [p,q] : taus) {
        CAPTURE(p);
        CAPTURE(q);
        testutil::set_tau(p, q);
        // Use a slightly reduced iteration count per τ to keep runtime reasonable
        testutil::pop_place_stress<61>(rng, PATH_TEST_STRESS_SCALE);
    }
}

TEST_CASE("Preview vs mutation across multiple τ values: B=61") {
    const std::pair<int,int> taus[] = {
        {0,1}, {1,3}, {1,2}, {2,3}, {1,1}
    };
    for (auto [p,q] : taus) {
        CAPTURE(p);
        CAPTURE(q);
        testutil::set_tau(p, q);

        using Bbits = core::bitset<61>;
        std::mt19937_64 rng = testutil::make_rng(0xABCDEULL ^ (p * 1315423911) ^ (q * 2654435761));
        Bbits unocc, colors;
        testutil::randomize_state(unocc, colors, rng);
        Path<61> path(unocc, colors);

        std::uniform_int_distribution<std::size_t> pos(0, 60);
        std::bernoulli_distribution color_choice(0.5);
        const std::size_t iterations = PATH_TEST_STRESS_SCALE;
        for (std::size_t it = 0; it < iterations; ++it) {
            const std::size_t idx = pos(rng);
            const std::size_t raw_idx = idx + 2;

            auto before_occ_raw     = graphs::test::raw_occ(path);
            auto before_colors_raw  = graphs::test::raw_colors(path);

            bool placed = false;
            bool new_color = false;
            bool expected_popped_color = false;

            Bbits expected_unocc = unocc;
            Bbits expected_colors = colors;

            if (unocc.test(idx)) {
                placed = true;
                new_color = color_choice(rng);
                expected_unocc.reset(idx);
                if (new_color) expected_colors.set(idx); else expected_colors.reset(idx);
            } else {
                expected_popped_color = colors.test(idx);
                expected_unocc.set(idx);
                expected_colors.reset(idx);
            }

            Path<61> preview(expected_unocc, expected_colors);
            auto proposed_occ_raw     = graphs::test::raw_occ(preview);
            auto proposed_colors_raw  = graphs::test::raw_colors(preview);
            auto proposed_unhappy_raw = graphs::test::raw_unhappy_cache(preview);

            if (placed) {
                path.place_agent(idx, new_color);
            } else {
                const bool popped_color = path.pop_agent(idx);
                CHECK(popped_color == expected_popped_color);
            }

            unocc = expected_unocc;
            colors = expected_colors;

            auto mutated_occ_raw     = graphs::test::raw_occ(path);
            auto mutated_colors_raw  = graphs::test::raw_colors(path);
            auto mutated_unhappy_raw = graphs::test::raw_unhappy_cache(path);

            auto occ_delta   = before_occ_raw;    occ_delta   ^= proposed_occ_raw;
            auto color_delta = before_colors_raw; color_delta ^= proposed_colors_raw;
            const std::size_t occ_ones   = occ_delta.count();
            const std::size_t color_ones = color_delta.count();
            const bool occ_ok   = (occ_delta.none()   || (occ_ones   == 1 && occ_delta.test(raw_idx)));
            const bool color_ok = (color_delta.none() || (color_ones == 1 && color_delta.test(raw_idx)));
            CHECK(occ_ok);
            CHECK(color_ok);

            CHECK(proposed_occ_raw == mutated_occ_raw);
            CHECK(proposed_colors_raw == mutated_colors_raw);
            CHECK(proposed_unhappy_raw == mutated_unhappy_raw);
        }
    }
}

// // -----------------------------------------------------------------------------
// // Sentinel boundary tests
// // -----------------------------------------------------------------------------

TEST_CASE("Sentinel raw bits toggle only at the left boundary slot") {
    // τ doesn't matter for raw toggling checks
    testutil::set_tau(1, 2);

    static constexpr std::size_t B = 8;
    static constexpr std::size_t PAD = 2;
    static constexpr std::size_t LEFT_SENT_RAW = PAD - 1; // == 1

    core::bitset<B> unocc, colors;
    unocc.reset(); colors.reset(); // start fully occupied color 0

    Path<B> path(unocc, colors);

    // Snapshot before
    auto occ_before = graphs::test::raw_occ(path);
    auto col_before = graphs::test::raw_colors(path);

    // Turn sentinel ON with color 1
    path.set_sentinel(/*occ*/1, /*col*/1);

    auto occ_after1 = graphs::test::raw_occ(path);
    auto col_after1 = graphs::test::raw_colors(path);

    // Occupancy delta: exactly one bit at LEFT_SENT_RAW
    auto occ_delta1 = occ_before; occ_delta1 ^= occ_after1;
    CHECK(occ_delta1.count() == 1);
    CHECK(occ_delta1.test(LEFT_SENT_RAW));

    // Color delta: exactly one bit at LEFT_SENT_RAW
    auto col_delta1 = col_before; col_delta1 ^= col_after1;
    CHECK(col_delta1.count() == 1);
    CHECK(col_delta1.test(LEFT_SENT_RAW));

    // Turn sentinel OFF (color should clear as well)
    path.set_sentinel(/*occ*/0, /*col*/0);
    auto occ_after2 = graphs::test::raw_occ(path);
    auto col_after2 = graphs::test::raw_colors(path);

    // Back to the original raw states
    CHECK(occ_after2 == occ_before);
    CHECK(col_after2 == col_before);
}

TEST_CASE("Sentinel acts as a left neighbor for v=0 (τ = 1/2)") {
    // τ = 1/2 → with exactly one neighbor:
    //   - match: 0/1 <= τ → NOT unhappy
    //   - mismatch: 1/1  > τ → unhappy
    testutil::set_tau(1, 2);

    static constexpr std::size_t B = 5;
    core::bitset<B> unocc, colors;

    // Prepare state with only v=0 occupied; all others unoccupied
    unocc.reset(); colors.reset();
    for (std::size_t i = 1; i < B; ++i) unocc.set(i);

    for (bool v0_color : {false, true}) {
        core::bitset<B> u = unocc, c = colors;
        if (v0_color) c.set(0);
        Path<B> path(u, c);

        // No sentinel → deg(0) = (occ[-1]=0) + (occ[1]=0) = 0 → never unhappy
        path.set_sentinel(0, 0);
        CHECK(path.is_unhappy(0) == false);
        CHECK(path.unhappy_count() == 0);

        // Sentinel occupied with SAME color → one like neighbor → not unhappy
        path.set_sentinel(/*occ*/1, /*col*/(v0_color ? 1 : 0));
        CHECK(path.is_unhappy(0) == false);
        CHECK(path.unhappy_count() == 0);

        // Sentinel occupied with OPPOSITE color → one mismatching neighbor → unhappy
        path.set_sentinel(/*occ*/1, /*col*/(v0_color ? 0 : 1));
        CHECK(path.is_unhappy(0) == true);
        CHECK(path.unhappy_count() == 1);

        // get_unhappy() should always return 0 in this configuration
        std::mt19937_64 rng = testutil::make_rng(0x515E77ULL ^ v0_color);
        for (int k = 0; k < 128; ++k) {
            const std::size_t idx = path.get_unhappy(rng);
            REQUIRE(idx < B);
            CHECK(idx == 0);
        }
    }
}

TEST_CASE("Sentinel effect is localized (only v=-1 and v=0 raw slots can flip in unhappy cache)") {
    testutil::set_tau(1, 2);

    static constexpr std::size_t B   = 9;
    static constexpr std::size_t PAD = 2;
    static constexpr std::size_t RAW = B + 2 * PAD;
    static constexpr std::size_t LEFT_SENT_RAW = PAD - 1; // 1
    static constexpr std::size_t V0_RAW        = PAD + 0; // 2

    // Construct a state with some far-away activity to ensure nothing else flips spuriously.
    core::bitset<B> unocc, colors;
    unocc.reset(); colors.reset();
    // Occupy a small island away from the left border
    unocc.set(4);                 // a gap
    colors.set(6);                // v=6 color 1, v=5 color 0 by default

    Path<B> path(unocc, colors);

    // Baseline snapshot
    auto unhappy_before = graphs::test::raw_unhappy_cache(path);

    // Toggle sentinel to a few combinations and confirm locality
    const std::pair<int,int> toggles[] = {{1,0}, {1,1}, {0,0}, {1,1}, {0,0}};
    for (auto [occ,col] : toggles) {
        path.set_sentinel(occ, col);
        auto unhappy_after = graphs::test::raw_unhappy_cache(path);
        auto delta = unhappy_before; delta ^= unhappy_after;

        // Any differences must be within {LEFT_SENT_RAW, V0_RAW}
        // (i.e., only the sentinel slot and vertex 0 can be affected)
        for (std::size_t i = 0; i < RAW; ++i) {
            if (delta.test(i)) {
                bool t1 = i == LEFT_SENT_RAW;
                bool t2 = i == V0_RAW;
                CHECK((t1 || t2));
            }
        }
        unhappy_before = unhappy_after;
    }
}

TEST_CASE("Sentinel does not affect counts or sampling domain") {
    testutil::set_tau(1, 2);

    static constexpr std::size_t B = 7;
    core::bitset<B> unocc, colors;
    unocc.reset(); colors.reset();

    // Make exactly one unoccupied vertex far from the border
    for (std::size_t i = 0; i < B; ++i) unocc.reset(i);
    unocc.set(5); // the unique hole
    colors.set(0); // arbitrary colors elsewhere

    Path<B> path(unocc, colors);

    const std::size_t base_gaps = path.count_by_color(std::nullopt);

    // Toggling sentinel must not change the number of unoccupied sites
    path.set_sentinel(1, 0);
    CHECK(path.count_by_color(std::nullopt) == base_gaps);
    path.set_sentinel(1, 1);
    CHECK(path.count_by_color(std::nullopt) == base_gaps);
    path.set_sentinel(0, 0);
    CHECK(path.count_by_color(std::nullopt) == base_gaps);

    // get_unoccupied() must remain within [0, B-1] and should repeatedly hit the only hole (index 5)
    std::mt19937_64 rng = testutil::make_rng(0xD00D5EEDULL);
    for (int t = 0; t < 128; ++t) {
        const std::size_t idx = path.get_unoccupied(rng);
        REQUIRE(idx < B);
        CHECK(idx == 5);
    }
}

TEST_CASE("Sentinel + first real neighbor interplay matches Schelling rule across τ") {
    // We exercise τ in {1/3, 1/2, 2/3} to cover transitions with deg=1 and deg=2.
    const std::pair<int,int> taus[] = { {1,3}, {1,2}, {2,3} };

    static constexpr std::size_t B = 4;
    for (auto [p, q] : taus) {
        CAPTURE(p); CAPTURE(q);
        testutil::set_tau(p, q);

        for (bool v0_color : {false, true}) {
            for (bool v1_occ : {false, true}) {
                for (bool v1_color : {false, true}) {
                    for (bool sent_occ : {false, true}) {
                        for (bool sent_color : {false, true}) {
                            core::bitset<B> unocc, colors;
                            unocc.reset(); colors.reset();   // start full c0

                            // v0: occupied with color v0_color
                            if (v0_color) colors.set(0);

                            // v1: maybe occupied, with color v1_color
                            if (!v1_occ) unocc.set(1);            // mark unoccupied if chosen
                            if (v1_occ && v1_color) colors.set(1); // else occupied color 0 by default

                            // everyone else unoccupied (to isolate influence)
                            for (std::size_t i = 2; i < B; ++i) unocc.set(i);

                            Path<B> path(unocc, colors);
                            path.set_sentinel(sent_occ ? 1 : 0, sent_color ? 1 : 0);

                            // ---- Expected for v0 (neighbors: left sentinel if present, right v1 if present) ----
                            std::uint32_t deg0 = 0, like0 = 0;
                            if (sent_occ) { ++deg0; like0 += (sent_color == v0_color) ? 1u : 0u; }
                            if (v1_occ)   { ++deg0; like0 += (v1_color   == v0_color) ? 1u : 0u; }
                            const bool expected_v0 = core::schelling::is_unhappy(deg0 - like0, deg0);

                            // ---- Expected for v1 (if occupied): neighbor is v0 only; v2 is unoccupied here) ----
                            bool expected_v1 = false;
                            if (v1_occ) {
                                const std::uint32_t deg1 = 1; // v0 is occupied by construction
                                const std::uint32_t like1 = (v1_color == v0_color) ? 1u : 0u;
                                expected_v1 = core::schelling::is_unhappy(deg1 - like1, deg1);
                            }

                            // ---- Checks ----
                            CAPTURE(v0_color);
                            CAPTURE(v1_occ);
                            CAPTURE(v1_color);
                            CAPTURE(sent_occ);
                            CAPTURE(sent_color);
                            CAPTURE(deg0);
                            CAPTURE(like0);

                            CHECK(path.is_unhappy(0) == expected_v0);
                            if (v1_occ) {
                                CHECK(path.is_unhappy(1) == expected_v1);
                            } else {
                                CHECK(path.is_unhappy(1) == false);
                            }

                            const std::size_t expected_total =
                                (expected_v0 ? 1u : 0u) + ((v1_occ && expected_v1) ? 1u : 0u);
                            CHECK(path.unhappy_count() == expected_total);

                            // ---- Optional: sampling sanity (only returns from the unhappy set) ----
                            if (expected_total > 0) {
                                std::mt19937_64 rng = testutil::make_rng(
                                    0xC0FFEEULL ^ (p * 0x9E3779B185EBCA87ULL) ^ (q * 0xC2B2AE3D27D4EB4FULL) ^
                                    (v0_color ? 0x5555ULL : 0) ^ (v1_occ ? 0xAAAAULL : 0) ^
                                    (v1_color ? 0x3333ULL : 0) ^ (sent_occ ? 0x7777ULL : 0) ^
                                    (sent_color ? 0xDEADULL : 0));

                                for (int t = 0; t < 64; ++t) {
                                    const std::size_t idx = path.get_unhappy(rng);
                                    REQUIRE(idx < B);
                                    bool first = idx == 0;
                                    bool second = first || (idx == 1 && v1_occ && expected_v1);
                                    CHECK(second);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
