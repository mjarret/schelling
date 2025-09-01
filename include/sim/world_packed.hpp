#pragma once
/*
World: bit-packed occupancy + type for binary agents, plus dynamic lists
for O(1) random selection of empty/occupied vertices.

Encoding:
  - occ[v] = 1 if occupied, 0 if empty
  - typ[v] = 1 if type=1, 0 if type=0 (ignored if occ[v]==0)

We also maintain:
  - vector<size_t> empties, occupied
  - position maps pos_empty[v], pos_occupied[v] for O(1) swap-removal updates

RATIONALE (2025-09-01):
- Removed unused locals that triggered -Wall/-Wextra warnings.
- Hardened removal logic to check position maps before touching containers, so
  calling set_occupied() on a vertex that hasn't been inserted into `empties`
  (common during initial seeding after resize) is safe.
*/

#include <vector>
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <stdexcept>
#include <limits>
#include <cassert>

#include "../util/bitset_vector.hpp"
#include "../rng/splitmix64.hpp"

struct WorldPacked {
    static constexpr std::size_t NPOS = std::numeric_limits<std::size_t>::max();

    std::size_t N{0};
    BitsetVector occ, typ;
    std::vector<std::size_t> empties, occupied;
    std::vector<std::size_t> pos_empty, pos_occupied;

    void resize(std::size_t n) {
        N = n;
        occ.resize(n);
        typ.resize(n);
        empties.clear(); empties.reserve(n);
        occupied.clear(); occupied.reserve(n);
        pos_empty.assign(n, NPOS);
        pos_occupied.assign(n, NPOS);
    }

    inline bool is_occ(std::size_t v) const { return occ.get(v); }
    inline bool type1(std::size_t v)  const { return typ.get(v); }

    // Mark vertex v as empty; update containers and maps.
    void set_empty(std::size_t v) {
        // If currently marked occupied, remove from occupied vector (if present).
        if (occ.get(v)) {
            const std::size_t pos = pos_occupied[v];
            if (pos != NPOS) {
                const std::size_t last_idx = occupied.size() - 1;
                const std::size_t moved = occupied[last_idx];
                std::swap(occupied[pos], occupied[last_idx]);
                pos_occupied[moved] = pos;
                occupied.pop_back();
                pos_occupied[v] = NPOS;
            } else {
                // Shouldn't happen in normal use, but keep state consistent.
                pos_occupied[v] = NPOS;
            }
        }
        occ.set(v, false);

        // Ensure v is listed among empties exactly once.
        if (pos_empty[v] == NPOS) {
            pos_empty[v] = empties.size();
            empties.push_back(v);
        }
    }

    // Mark vertex v as occupied with type t1; update containers and maps.
    void set_occupied(std::size_t v, bool t1) {
        // If currently marked empty, remove from empties vector (if present).
        if (!occ.get(v)) {
            const std::size_t pos = pos_empty[v];
            if (pos != NPOS) {
                const std::size_t last_idx = empties.size() - 1;
                const std::size_t moved = empties[last_idx];
                std::swap(empties[pos], empties[last_idx]);
                pos_empty[moved] = pos;
                empties.pop_back();
                pos_empty[v] = NPOS;
            }
        }

        occ.set(v, true);
        typ.set(v, t1);

        // Ensure v is listed among occupied exactly once.
        if (pos_occupied[v] == NPOS) {
            pos_occupied[v] = occupied.size();
            occupied.push_back(v);
        }
    }

    // Random selections (caller must ensure container non-empty).
    inline std::size_t random_empty(SplitMix64& rng) const {
        assert(!empties.empty());
        return empties[rng.uniform_index(empties.size())];
    }
    inline std::size_t random_occupied(SplitMix64& rng) const {
        assert(!occupied.empty());
        return occupied[rng.uniform_index(occupied.size())];
    }
};
