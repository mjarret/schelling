#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <utility>

#include "core/bitset.hpp"
#include "core/config.hpp"
#include "core/schelling_threshold.hpp"

namespace graphs {

/**
 * @brief Two-color clique (plus unoccupied), counts-first (no ghosts).
 */
template <std::size_t Size>
class Clique {
public:
    using count_t = std::uint64_t;
    using count_t_opt = std::optional<count_t>;
    using bitset = core::bitset<Size>;
    using sample_from = std::uniform_int_distribution<count_t>;
    using index_t = std::size_t;

    explicit Clique() = default;
    explicit Clique(const bitset& occ, const bitset& color) noexcept
        : c0_(static_cast<count_t>(occ.count() - color.count()))
        , c1_(static_cast<count_t>(color.count())) {}

    explicit Clique(count_t c0, count_t c1) noexcept
        : c0_(c0), c1_(c1) {}

    inline std::optional<bool> pop_agent(index_t from) {
        CORE_ASSERT_H(from < occupied_count(), "Clique::pop_agent: index out of range");
        if(from < c0_) { c0_--; } else { c1_--; }
        return from >= c0_;
    }

    void place_agent(index_t, bool color) noexcept {
        CORE_ASSERT_H(occupied_count() < Size, "Clique::place_agent: clique is full");
        c0_ += !color;
        c1_ += color;
    }

    inline bool get_color(count_t v) const noexcept { return v >= c0_; }

    inline count_t count_by_color(std::optional<bool> c = std::nullopt) const noexcept {
        if (c == std::nullopt) return Size - (c0_ + c1_);
        return c.value() ? c1_ : c0_;
    }

    template<class URBG>
    inline count_t get_unoccupied(URBG& rng) const noexcept {  
        CORE_ASSERT_H(occupied_count() < Size, "Clique::get_unoccupied: no unoccupied vertices");
        return sample_from(occupied_count(), Size - 1)(rng);
    }
 
    template<class URBG>
    inline count_t_opt get_unhappy(URBG& rng) const noexcept {
        const auto [w0, w1] = unhappy_weights();
        CORE_ASSERT_H((w0 + w1) > 0, "Clique::get_unhappy: no unhappy vertices");
        return sample_from(0, w0 + w1 - 1)(rng) + (w0 ? 0 : c0_);
    }

    inline count_t unhappy_count() const noexcept {
        const auto [w0, w1] = unhappy_weights();
        return w0 + w1;
    }

    inline bool is_unhappy(count_t v) const noexcept {
        CORE_ASSERT_H(v < Size, "Clique::is_unhappy: index out of range");
        return 
            core::schelling::is_unhappy((v < c0_ + c1_) ? c1_ : c0_, occupied_count() - 1);
    }

    inline count_t occupied_count() const noexcept { return c0_ + c1_; }
    
private:
    std::pair<count_t, count_t> unhappy_weights() const noexcept {
        const count_t neigh = c0_ + c1_ - 1;
        const bool u0 = core::schelling::is_unhappy(c1_, neigh);
        const bool u1 = core::schelling::is_unhappy(c0_, neigh);
        return { u0 ? c0_ : 0, u1 ? c1_ : 0 };
    }

    count_t c0_{0};
    count_t c1_{0};
};

} // namespace graphs
