#pragma once
#include <vector>
#include <mutex>
#include <cstdint>

class MovesHistogram {
public:
    void add(uint64_t x) {
        std::lock_guard<std::mutex> lk(mx_);
        vals_.push_back(x);
    }
    std::vector<uint64_t> snapshot() const {
        std::lock_guard<std::mutex> lk(mx_);
        return vals_;
    }
private:
    mutable std::mutex mx_;
    std::vector<uint64_t> vals_;
};
