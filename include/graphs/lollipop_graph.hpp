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
    
        uint64_t local_frustration(size_t v) const {
            if (v < bridge) {
                // Clique vertex: its clique-internal disagreements + possible disagreement with bridge
                uint64_t d = clique.local_frustration(v);
                const std::uint32_t cv = static_cast<std::uint32_t>(clique.get_color(v));
                d += (bridge_color != 0 && cv != 0 && cv != bridge_color);
                return d;
            } else if (v == bridge) {
                // Bridge vertex: disagreements to all clique vertices + to first path vertex
                if (bridge_color == 0) return 0;
                uint64_t d = clique.total_occupied() - clique.color_count(static_cast<uint32_t>(bridge_color));
                if (n_path > 0) {
                    const std::uint32_t c0 = path.get_color(0);
                    d += (c0 != 0 && c0 != bridge_color);
                }
                return d;
            } else {
                // Path vertex: path-internal disagreements + possible disagreement with bridge (for the first path vertex)
                const size_t idx = v - n_clique;
                uint64_t d = path.local_frustration(idx);
                if (idx == 0) {
                    const std::uint32_t c0 = path.get_color(0);
                    d += (bridge_color != 0 && c0 != 0 && c0 != bridge_color);
                }
                return d;
            }
        }

        // Compute total frustration by summing subgraph totals and the bridge edges.
        // No memoization at the lollipop level; subgraphs may memoize internally.
        uint64_t total_frustration() const {
            return clique.total_frustration() + path.total_frustration() + bridge_frustration();
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
            // Invalidate base memoized total
            this->tf = static_cast<std::uint64_t>(-1);
        }
    
        private:
        size_t n_clique;
        size_t bridge = n_clique -1;    // Handle bridge distinctly from rest of clique/path
        size_t bridge_color = 0;
        size_t n_path;
 
        CliqueGraph<K> clique;        
        PathGraph<K> path;

        uint64_t bridge_frustration() const {
            if (bridge_color == 0) return 0;
            uint64_t bf = clique.total_occupied() - clique.color_count(static_cast<uint32_t>(bridge_color));
            bf += (path.get_color(0) != bridge_color)*(path.get_color(0) != 0);
            return bf;
        }
};

} // namespace graphs
