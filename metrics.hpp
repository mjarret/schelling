#pragma once
#include <cstdint>
#include <utility>

struct Threshold {
    // Rational p/q threshold to avoid FP drift; default 0.5
    uint32_t p, q;
    explicit Threshold(double frac = 0.5) {
        const uint32_t Q = 1000;
        p = static_cast<uint32_t>(frac * Q + 0.5);
        q = Q;
    }
    Threshold(uint32_t P, uint32_t Q) : p(P), q(Q) {}
    inline bool satisfied(uint32_t same, uint32_t other) const {
        const uint32_t denom = same + other;
        if (denom == 0) return true; // convention
        return (uint64_t(same) * q) >= (uint64_t(p) * denom);
    }
};

template <class World, class Geometry>
struct Metrics {
    const World&    W;
    const Geometry& G;
    explicit Metrics(const World& world, const Geometry& geom) : W(world), G(geom) {}

    inline void neighbor_counts(uint32_t v, uint32_t my_type, uint32_t& same, uint32_t& other) const {
        same = other = 0;
        G.for_each_neighbor(v, [&](uint32_t u){
            if (!W.occupied(u)) return;
            uint32_t t = W.type_of(u);
            if (t == my_type) ++same; else ++other;
        });
    }

    // Returns (#unhappy agents, avg same-type neighbor fraction)
    std::pair<uint32_t, double> unhappy_and_avg_same_fraction(const Threshold& th) const {
        uint64_t    unhappy = 0;
        long double sum_frac = 0.0L, count = 0.0L;

        W.for_each_agent([&](uint32_t v, uint32_t t){
            uint32_t s=0, o=0;
            neighbor_counts(v, t, s, o);
            const uint32_t denom = s + o;
            double frac = (denom == 0) ? 1.0 : double(s) / double(denom);
            sum_frac += frac;
            if (!th.satisfied(s, o)) ++unhappy;
            ++count;
        });
        double avg = (count == 0.0L) ? 0.0 : double(sum_frac / count);
        return { static_cast<uint32_t>(unhappy), avg };
    }
};
