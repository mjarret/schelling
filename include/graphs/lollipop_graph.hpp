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

        LollipopGraph(size_t n_clique_, size_t n_path_)
            : n_clique(n_clique_), bridge(n_clique_ ? n_clique_ - 1 : 0), bridge_color(0), n_path(n_path_),
              clique(n_clique_ ? n_clique_ - 1 : 0), path(n_path_) {
            num_vertices = n_clique + n_path;
        }
    
        uint64_t local_frustration(size_t v) const {
            if(v < bridge) {
                return clique.local_frustration(v);
            } else if (v == bridge) {
                return bridge_frustration();
            } else {
                return path.local_frustration(v - n_clique);
            }
        }

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
