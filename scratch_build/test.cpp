#include <array>
#include <vector>
#include <cstdint>
#include "include/graphs/path_graph.hpp"
#include "include/graphs/clique_graph.hpp"
#include "include/graphs/lollipop_graph.hpp"

int main(){
    graphs::PathGraph<2> pg(10);
    pg.change_color(0, 1u, 0u);
    auto tf1 = pg.total_frustration();

    graphs::CliqueGraph<2> cg(5);
    std::array<std::uint64_t, 3> cnt{}; cnt[0]=2; cnt[1]=2; cnt[2]=1;
    cg.set_colors(cnt);
    auto tf2 = cg.total_frustration();

    graphs::LollipopGraph<2> lg(3, 5);
    lg.change_color(0,1u,0u);
    auto tf3 = lg.total_frustration();
    return (tf1 + tf2 + tf3) == 0;
}
