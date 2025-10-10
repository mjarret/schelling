#pragma once

#include <cstddef>
#include <cstring>
#include <random>
#include <type_traits>

#include "core/bitset.hpp"
#include "core/config.hpp"

namespace graphs {
namespace detail {

template<std::size_t B, std::size_t Padding = 2, bool SentinelsFilled = false>
class PaddedBitset {
    using bitset = core::bitset<B + 2 * Padding>;
    template<std::size_t, std::size_t, bool>
    friend class PaddedBitset;

public:
    PaddedBitset() noexcept = default;

    explicit PaddedBitset(const core::bitset<B>& bs) noexcept {
        data_ = bs;            // cross-size assignment supported by backend
        data_ <<= Padding;     // shift into active window
        apply_sentinels();
        update_count_cache();
    }

    template<std::size_t OB, std::enable_if_t<OB != B, int> = 0>
    explicit PaddedBitset(const core::bitset<OB>& bs) noexcept {
        constexpr std::size_t word_bits = sizeof(CORE_BITSET_WORD_T) * 8;
        constexpr std::size_t dst_words = (B + 2 * Padding + word_bits - 1) / word_bits;
        constexpr std::size_t src_words = (OB + word_bits - 1) / word_bits;
        constexpr std::size_t copy_words = (src_words < dst_words) ? src_words : dst_words;

        if constexpr (copy_words != 0) {
            std::memcpy(data_.data(), bs.data(), copy_words * sizeof(CORE_BITSET_WORD_T));
        }
        if constexpr (copy_words < dst_words) {
            std::memset(data_.data() + copy_words, 0, (dst_words - copy_words) * sizeof(CORE_BITSET_WORD_T));
        }

        if constexpr (OB != (B + 2 * Padding)) {
            data_ <<= Padding;
        }
        apply_sentinels();
        update_count_cache();
    }

    template<bool OtherSentinels, std::enable_if_t<OtherSentinels != SentinelsFilled, int> = 0>
    PaddedBitset(const PaddedBitset<B, Padding, OtherSentinels>& other) noexcept {
        data_ = other.data_;
        apply_sentinels();
        update_count_cache();
    }

    inline bool operator[](std::size_t idx) const noexcept { return data_[idx + Padding]; }
    inline bool operator==(const PaddedBitset& other) const noexcept { return data_ == other.data_; }
    inline bool operator!=(const PaddedBitset& other) const noexcept { return data_ != other.data_; }

    inline void reset(std::size_t idx) noexcept {
        const std::size_t raw = idx + Padding;
        if (raw >= Padding && raw < Padding + B) {   // inside active window
            count_cache_ -= data_[raw];
        } else {
            padding_ones_left -= data_[raw];
        }
        data_.reset(raw);
    }

    inline void set(std::size_t idx) noexcept {
        const std::size_t raw = idx + Padding;
        if (raw >= Padding && raw < Padding + B) {   // inside active window
            count_cache_ += !data_[raw];
        } else {
            padding_ones_left += !data_[raw];
        }
        data_.set(raw);
    }

    inline std::size_t count() const noexcept { return count_cache_; }

    [[nodiscard]] inline operator core::bitset<B>() const noexcept {
        bitset shifted = data_;
        shifted >>= Padding;
        shifted.reset_range(B, B + 2 * Padding);

        core::bitset<B> compact;
        constexpr std::size_t word_bits = sizeof(CORE_BITSET_WORD_T) * 8;
        constexpr std::size_t word_count = (B + word_bits - 1) / word_bits;
        if constexpr (word_count != 0) {
            std::memcpy(compact.data(), shifted.data(), word_count * sizeof(CORE_BITSET_WORD_T));
            if constexpr ((B % word_bits) != 0) {
                auto* buf = compact.data();
                constexpr CORE_BITSET_WORD_T mask = (CORE_BITSET_WORD_T(1) << (B % word_bits)) - 1;
                buf[word_count - 1] &= mask;
            }
        }
        return compact;
    }

    friend inline PaddedBitset operator~(const PaddedBitset& a) noexcept {
        return PaddedBitset(~a.data_);
    }
    friend inline PaddedBitset operator&(const PaddedBitset& a, const PaddedBitset& b) noexcept {
        return PaddedBitset(a.data_ & b.data_);
    }
    friend inline PaddedBitset operator|(const PaddedBitset& a, const PaddedBitset& b) noexcept {
        return PaddedBitset(a.data_ | b.data_);
    }
    friend inline PaddedBitset operator^(const PaddedBitset& a, const PaddedBitset& b) noexcept {
        return PaddedBitset(a.data_ ^ b.data_);
    }
    friend inline PaddedBitset operator<<(const PaddedBitset& a, std::size_t s) noexcept {
        return PaddedBitset(a.data_ << s);
    }
    friend inline PaddedBitset operator>>(const PaddedBitset& a, std::size_t s) noexcept {
        return PaddedBitset(a.data_ >> s);
    }

    inline const bitset& raw() const noexcept { return data_; }

    template<class URBG>
    std::size_t random_setbit_index(URBG& rng) const noexcept {
        std::uniform_int_distribution<std::size_t> pick(0, count() - 1u);
        CAPTURE(padding_ones_left);
        const std::size_t k = pick(rng) + padding_ones_left;
        auto ret = data_.kth_one(k) - Padding;
        REQUIRE(ret < B);
        return ret;
    }

    template<class URBG>
    std::size_t random_unsetbit_index(URBG& rng) const noexcept {
        std::uniform_int_distribution<std::size_t> pick(0, B - count() - 1u);
        CAPTURE(padding_ones_left);
        const std::size_t k = pick(rng) + (Padding - padding_ones_left);
        auto ret = data_.kth_zero(k) - Padding;
        REQUIRE(ret < B);
        return data_.kth_zero(k) - Padding;
    }

private:
    inline void apply_sentinels() noexcept {
        if constexpr (Padding == 0) {
            return;
        }
        if constexpr (SentinelsFilled) {
            data_.set_range(0, Padding - 1);
            data_.set_range(B + Padding, B + 2 * Padding - 1);
        } else {
            data_.reset_range(0, Padding - 1);
            data_.reset_range(B + Padding, B + 2 * Padding - 1);
        }
    }

    inline void update_count_cache() noexcept {
        count_cache_ = data_.count();
        if constexpr (Padding != 0) {
            for (std::size_t i = 0; i < Padding; ++i) {
                count_cache_ -= data_.test(i);
                count_cache_ -= data_.test(B + Padding + i);
            }
        }
    }

    bitset data_{};
    std::size_t count_cache_ = static_cast<std::size_t>(-1);
    std::size_t padding_ones_left{0};
};

} // namespace detail
} // namespace graphs
