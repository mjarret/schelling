#pragma once

#include <cstdint>
#include <array>
#include <cassert>
#include "core/graph_base.hpp"
#include "clique_graph.hpp"
#include "path_graph.hpp"

namespace graphs {

template <std::size_t K>
class LollipopGraph : public GraphBase<LollipopGraph<K>> {
    public:
        using GraphBase::tf;
        using GraphBase::counts;

        LollipopGraph(size_t n_clique, size_t n_path)
            : n_clique(n_clique), n_path(n_path) {}
    
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
        
        void change_color(index_t v, uint32_t c, uint32_t c_original = get_color(v)) {
            if(v < bridge) {
                clique.change_color(v, c, c_original);
            } else if (v == bridge) {
                tf -= bridge_frustration(); // remove old contribution
                bridge_color = c;
                tf += bridge_frustration(); // add new contribution
            } else {
                path.change_color(v - n_clique, c, c_original);
            }
        }
    
        private:
        size_t n_clique;
        size_t bridge = n_clique -1;    // Handle bridge distinctly from rest of clique/path
        size_t bridge_color = 0;
        size_t n_path;
 
        CliqueGraph<K> clique = CliqueGraph<K>(n_clique-1);        
        PathGraph<K> path = PathGraph<K>(n_path);

        uint64_t bridge_frustration() const {
            if (bridge_color == 0) return 0;
            uint64_t bf = clique.total_occupied() - clique.counts[bridge_color];
            bf += (path.get_color(0) != bridge_color)*(path.get_color(0) != 0);
            return bf;
        }
};

} // namespace graphs
