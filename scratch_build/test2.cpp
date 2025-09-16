#include <cstdint>
#include <array>
#include "include/graphs/path_graph.hpp"
#include "include/graphs/lollipop_graph.hpp"
#include "include/core/rng.hpp"
#include "include/sim/schelling_next.hpp"

/**
 * @brief Scratch entry for quick checks.
 */
int main(){
    core::Xoshiro256ss rng(123);
    graphs::PathGraph<2> pg(10);
    pg.change_color(1, 1u, 0);
    pg.change_color(2, 2u, 0);
    auto d = sim::schelling_next(pg, rng);
    (void)d;
    graphs::LollipopGraph<2> lg(3, 5);
    lg.change_color(0, 1u, 0);
    auto d2 = sim::schelling_next(lg, rng);
    (void)d2;
    return 0;
}
