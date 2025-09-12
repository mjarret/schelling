#pragma once

#include <cstdint>
#include <array>
#include <cassert>
#include "graphs/core/graph_base.hpp"
#include "clique_graph.hpp"
#include "path_graph.hpp"

namespace graphs {

template <std::size_t K>
class LollipopGraph : public GraphBase<LollipopGraph<K>, K> {
    public:
        using Base = GraphBase<LollipopGraph<K>, K>;
        using Base::num_vertices;
        using Base::tf;

        LollipopGraph(size_t n_clique_, size_t n_path_)
            : n_clique(n_clique_), bridge(n_clique_ ? n_clique_ - 1 : 0), bridge_color(0), n_path(n_path_),
              clique(n_clique_ ? n_clique_ - 1 : 0), path(n_path_) {
            num_vertices = n_clique + n_path;
        }
    
        // Local disagreements without scanning full adjacency.
        // Layout:
        //   core clique = [0 .. bridge-1], bridge = (n_clique-1), path = [n_clique .. n_clique+n_path-1]
        // CliqueGraph<K> here models only the core (excludes the bridge), so we explicitly
        // add bridge-related terms where appropriate.
        uint64_t local_frustration(size_t v) const {
            if (v < bridge) {
                // Core clique vertex: clique-internal + possible disagreement with bridge (0/1)
                const std::uint32_t cv = static_cast<std::uint32_t>(clique.get_color(v));
                return clique.local_frustration(v)
                     + static_cast<std::uint64_t>((bridge_color != 0u) * (cv != 0u) * (cv != bridge_color));
            } else if (v == bridge) {
                // Bridge vertex: disagreements to all clique vertices + to first path vertex
                const std::uint32_t p0 = path.get_color(0);
                return clique.total_occupied()
                     - static_cast<std::uint64_t>(clique.color_count(static_cast<std::uint32_t>(bridge_color)))
                     + static_cast<std::uint64_t>((p0 != 0u) * (p0 != bridge_color));
            } else {
                // Path vertex: path-local + possible disagreement with bridge only at first path vertex
                const std::uint32_t pc = path.get_color(v - n_clique);
                return path.local_frustration(v - n_clique)
                     + static_cast<std::uint64_t>((v == bridge + 1u) * (bridge_color != 0u) * (pc != 0u) * (pc != bridge_color));
            }
        }

        // Compute total frustration by summing subgraph totals and the bridge edges.
        // No memoization at the lollipop level; subgraphs may memoize internally.
        uint64_t total_frustration() const {
            return clique.total_frustration() + path.total_frustration() + bridge_frustration();
        }

        // Occupied neighbor count specialized for hot calls from the runner.
        // NOTE: can be called on any v; for clique-core we subtract self occupancy explicitly.
        // Keep this O(1) and branch-lean; do not replace with scans.
        std::uint32_t occupied_neighbor_count(index_t v) const {
            if (v < bridge) {
                return clique.total_occupied() -(get_color(v) != 0) + (bridge_color != 0);
            } else if (v == bridge) {
                return clique.total_occupied() + (path.get_color(0) != 0);
            } else {
                return path.occupied_neighbor_count(v - n_clique) + (bridge_color != 0)*(v == bridge + 1);
            }
        }

        uint64_t get_color(index_t v) const {
            if(v < bridge) {
                return clique.get_color(v);
            } else if (v == bridge) {
                return bridge_color;
            } else {
                return path.get_color(v - n_clique);
            }
        }
        
        void change_color(index_t v, uint32_t c, uint32_t c_original = 0) {
            if (c_original == 0) c_original = static_cast<uint32_t>(get_color(v));
            if(v < bridge) {
                clique.change_color(v, c, c_original);
            } else if (v == bridge) {
                bridge_color = c;
            } else {
                path.change_color(v - n_clique, c, c_original);
            }
        }
    
        private:
        size_t n_clique;
        size_t bridge = n_clique -1;    // Handle bridge distinctly from rest of clique/path
        size_t bridge_color = 0;
        size_t n_path;
 
        CliqueGraph<K> clique;        
        PathGraph<K> path;

        // Single bridge edge contribution used by total_frustration().
        uint64_t bridge_frustration() const {
            if (bridge_color == 0) return 0;
            uint64_t bf = clique.total_occupied() - clique.color_count(static_cast<uint32_t>(bridge_color));
            bf += (path.get_color(0) != bridge_color)*(path.get_color(0) != 0);
            return bf;
        }
};

} // namespace graphs
