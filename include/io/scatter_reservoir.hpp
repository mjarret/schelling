#pragma once
// include/io/scatter_reservoir.hpp
// Thread-safe reservoir sampling of (x,y) points for plotting.

#include <vector>
#include <mutex>
#include <cstddef>
#include "../rng/splitmix64.hpp"

class ScatterReservoir {
public:
    explicit ScatterReservoir(std::size_t capacity) : cap_(capacity), count_(0) {
        xs_.reserve(capacity); ys_.reserve(capacity);
    }

    void add(double x, double y, SplitMix64& rng) {
        std::lock_guard<std::mutex> lk(mu_);
        const std::size_t t = ++count_;
        if (xs_.size() < cap_) { xs_.push_back(x); ys_.push_back(y); return; }
        const std::size_t j = rng.uniform_index(t);
        if (j < cap_) { xs_[j] = x; ys_[j] = y; }
    }

    void snapshot(std::vector<double>& out_x, std::vector<double>& out_y) const {
        std::lock_guard<std::mutex> lk(mu_);
        out_x = xs_; out_y = ys_;
    }

private:
    std::size_t cap_;
    mutable std::mutex mu_;
    std::vector<double> xs_, ys_;
    std::size_t count_;
};
