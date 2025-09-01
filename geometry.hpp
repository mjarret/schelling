#pragma once
#include <cstdint>

struct Torus2D {
    uint32_t W, H;
    explicit Torus2D(uint32_t w, uint32_t h) : W(w), H(h) {}
    inline uint32_t N() const { return W * H; }
    inline uint32_t idx(uint32_t x, uint32_t y) const { return y * W + x; }
    inline void xy(uint32_t index, uint32_t& x, uint32_t& y) const { x = index % W; y = index / W; }

    template <typename F>
    inline void for_each_neighbor(uint32_t v, F&& f) const {
        uint32_t x, y; xy(v, x, y);
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) continue;
                uint32_t nx = (x + dx + W) % W;
                uint32_t ny = (y + dy + H) % H;
                f(idx(nx, ny));
            }
        }
    }
};

struct Lollipop {
    uint32_t tail_size, clique_size;
    explicit Lollipop(uint32_t t, uint32_t c) : tail_size(t), clique_size(c) {}
    inline uint32_t N() const {return tail_size + clique_size;}
};

