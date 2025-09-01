#pragma once
#include <cstddef>
#include <utility>

struct TorusGrid {
    using index_type = std::size_t;
    std::size_t W{0}, H{0};

    TorusGrid() = default;
    TorusGrid(std::size_t w, std::size_t h) : W(w), H(h) {}

    inline std::size_t num_vertices() const { return W * H; }
    inline std::size_t degree(std::size_t) const { return 4; }

    inline std::pair<std::size_t,std::size_t> xy(std::size_t v) const {
        return { v % W, v / W };
    }
    inline std::size_t idx(std::size_t x, std::size_t y) const {
        return y * W + x;
    }

    template<class F>
    inline void for_each_neighbor(std::size_t v, F&& f) const {
        auto [x,y] = xy(v);
        const std::size_t xm = (x + W - 1) % W;
        const std::size_t xp = (x + 1) % W;
        const std::size_t ym = (y + H - 1) % H;
        const std::size_t yp = (y + 1) % H;
        f(idx(xm, y));
        f(idx(xp, y));
        f(idx(x, ym));
        f(idx(x, yp));
    }
};
