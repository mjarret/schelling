#pragma once
/*
random_stepper.hpp — "Any" move rule that moves ONLY unhappy agents
Updated: 2025-09-01

BEHAVIOR
- Pick a random unhappy occupied vertex v_from (O(1) via Metrics pool).
- Pick a random empty vertex v_to.
- Move regardless of whether v_to will be happy or not (blind relocation).
- Update unhappy flags ONLY for affected vertices by scanning their neighbors
  on demand (no stored deg[]/occ_cnt[] arrays).

RUNTIME
- Work per move is proportional to degrees of neighbors of v_from and v_to,
  which are small in our graphs (4 on torus; modest on lollipop).

RETURNS
- true  : a relocation occurred
- false : no unhappy agents or no empty vertices (no move)
*/

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <cassert>

#include "world_packed.hpp"
#include "metrics.hpp"
#include "../rng/splitmix64.hpp"

template<class Graph>
struct RandomStepperAny {
    const Graph* G{nullptr};
    WorldPacked* W{nullptr};
    Metrics<Graph>* M{nullptr};

    void bind(const Graph* g, WorldPacked* w, Metrics<Graph>* m) {
        G = g; W = w; M = m;
    }

    bool step(SplitMix64& rng) {
        if (W->empties.empty() || !M->has_unhappy()) return false;

        // Select an unhappy agent uniformly at random.
        const std::size_t v_from = M->random_unhappy(rng);
        if (!W->is_occ(v_from)) return false; // guard: shouldn't happen
        const bool t1 = W->type1(v_from);

        // Pick a random empty destination.
        const std::size_t v_to = W->random_empty(rng);
        if (v_to == v_from) return false; // degenerate guard

        // --------- v_from becomes empty ----------
        // For each currently-occupied neighbor u of v_from, re-evaluate unhappy via scan.
        M->for_each_occ_neighbor(v_from, [&](std::size_t u){
            const bool was_unhappy = M->is_unhappy(u);
            // After v_from is emptied, u loses one occupied neighbor; we see that in the scan.
            const bool now_unhappy = M->recompute_unhappy_scan(u);
            if (was_unhappy != now_unhappy) M->set_unhappy(u, now_unhappy);
        });

        // v_from itself was unhappy by construction; it becomes empty so mark not-unhappy.
        if (M->is_unhappy(v_from)) M->set_unhappy(v_from, false);
        W->set_empty(v_from);

        // --------- v_to becomes occupied ----------
        // Place agent (type t1) and then update neighbors by scan.
        W->set_occupied(v_to, t1);

        M->for_each_occ_neighbor(v_to, [&](std::size_t u){
            const bool was_unhappy = M->is_unhappy(u);
            const bool now_unhappy = M->recompute_unhappy_scan(u);
            if (was_unhappy != now_unhappy) M->set_unhappy(u, now_unhappy);
        });

        // Finally, recompute unhappy for v_to itself with its new neighborhood.
        M->set_unhappy(v_to, M->recompute_unhappy_scan(v_to));

        return true;
    }
};
