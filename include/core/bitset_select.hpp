#pragma once

#include <vector>
#include <cstdint>
#include <limits>
#include <bit>

#if defined(__x86_64__) || defined(_M_X64)
#  include <immintrin.h>
#endif

namespace core {

// Lightweight block-Fenwick bitset supporting uniform random 1-bit selection
// and O(log^2) random 0-bit selection. Backed by contiguous 64-bit words.
template<std::size_t BlockBits = 256>
class BlockSelectBitset {
    static_assert(BlockBits % 64 == 0, "BlockBits must be a multiple of 64");
public:
    static constexpr std::size_t kBlockBits     = BlockBits;
    static constexpr std::size_t kWordBits      = 64;
    static constexpr std::size_t kWordsPerBlock = kBlockBits / kWordBits;
    static constexpr std::size_t npos           = std::numeric_limits<std::size_t>::max();

    explicit BlockSelectBitset(std::size_t nbits)
    : nbits_(nbits),
      nblocks_((nbits + kBlockBits - 1) / kBlockBits),
      words_(nblocks_ * kWordsPerBlock, 0),
      fenwick_(nblocks_ + 1, 0),
      total_ones_(0) {}

    std::size_t size_bits() const noexcept { return nbits_; }
    std::size_t count() const noexcept { return total_ones_; }

    bool test(std::size_t i) const noexcept {
        if (i >= nbits_) return false;
        auto [w, off] = word_index_(i);
        return (words_[w] >> off) & 1ULL;
    }
    void set(std::size_t i) noexcept { if (i < nbits_) write_bit_(i, true); }
    void reset(std::size_t i) noexcept { if (i < nbits_) write_bit_(i, false); }
    void flip(std::size_t i) noexcept { if (i < nbits_) toggle_bit_(i); }

    template<class URBG>
    std::size_t random_one(URBG& rng) const noexcept {
        if (total_ones_ == 0) return npos;
        std::size_t k = rng.uniform_index(total_ones_);
        std::size_t b1 = fenwick_find_by_order_(static_cast<std::uint32_t>(k + 1));
        std::uint32_t ones_before = fenwick_sum_(b1 - 1);
        std::uint32_t r_in_block  = static_cast<std::uint32_t>(k - ones_before);
        return select1_in_block_(b1, r_in_block);
    }

    template<class URBG>
    std::size_t random_zero(URBG& rng) const noexcept {
        const std::size_t total_zeros = nbits_ - total_ones_;
        if (total_zeros == 0) return npos;
        const std::size_t k = rng.uniform_index(total_zeros);
        // Binary search over blocks using ones Fenwick to derive zeros prefix.
        std::size_t lo = 1, hi = nblocks_;
        while (lo < hi) {
            std::size_t mid = (lo + hi) >> 1;
            std::uint64_t zeros_mid = prefix_bits_(mid) - fenwick_sum_(mid);
            if (zeros_mid > k) hi = mid; else lo = mid + 1;
        }
        std::uint64_t zeros_before = prefix_bits_(lo - 1) - fenwick_sum_(lo - 1);
        std::uint32_t r_in_block = static_cast<std::uint32_t>(k - zeros_before);
        return select0_in_block_(lo, r_in_block);
    }

private:
    std::size_t nbits_;
    std::size_t nblocks_;
    std::vector<std::uint64_t> words_;
    std::vector<std::uint32_t> fenwick_; // ones per block (1-based)
    std::size_t total_ones_;

    static constexpr std::pair<std::size_t,unsigned>
    word_index_(std::size_t i) noexcept { return { i / kWordBits, static_cast<unsigned>(i & (kWordBits - 1)) }; }
    static constexpr std::size_t word_base_in_block_(std::size_t b1) noexcept { return (b1 - 1) * kWordsPerBlock; }

    std::size_t block_capacity_bits_(std::size_t b1) const noexcept {
        std::size_t start = (b1 - 1) * kBlockBits;
        std::size_t end   = start + kBlockBits;
        if (end > nbits_) end = nbits_;
        return (end > start) ? (end - start) : 0;
    }
    std::uint64_t prefix_bits_(std::size_t upto_b1) const noexcept {
        std::uint64_t cap = static_cast<std::uint64_t>(kBlockBits) * upto_b1;
        if (cap > nbits_) cap = static_cast<std::uint64_t>(nbits_);
        return cap;
    }

    static inline void fenwick_add_(std::vector<std::uint32_t>& ft, std::size_t idx, int delta) noexcept {
        for (std::size_t i = idx; i < ft.size(); i += i & -i) ft[i] += static_cast<std::uint32_t>(delta);
    }
    inline std::uint32_t fenwick_sum_(std::size_t idx) const noexcept {
        std::uint32_t s = 0; for (std::size_t i = idx; i; i &= (i - 1)) s += fenwick_[i]; return s;
    }
    inline std::size_t fenwick_find_by_order_(std::uint32_t k1) const noexcept {
        const std::size_t n = nblocks_;
        std::size_t idx = 0;
#if defined(__cpp_lib_bitops) && __cpp_lib_bitops >= 201907L
        std::size_t bit = 1ull << (63 - std::countl_zero(n | 1ull));
#else
        std::size_t bit = std::size_t(1) << (8 * sizeof(std::size_t) - 1);
        while ((bit >> 1) > n) bit >>= 1;
#endif
        for (; bit; bit >>= 1) {
            std::size_t next = idx + bit;
            if (next <= n && fenwick_[next] < k1) { k1 -= fenwick_[next]; idx = next; }
        }
        return idx + 1;
    }

    static inline unsigned popcnt64_(std::uint64_t x) noexcept {
#if defined(__cpp_lib_bitops) && __cpp_lib_bitops >= 201907L
        return std::popcount(x);
#elif defined(__GNUC__) || defined(__clang__)
        return static_cast<unsigned>(__builtin_popcountll(x));
#else
        x -= (x >> 1) & 0x5555555555555555ull;
        x = (x & 0x3333333333333333ull) + ((x >> 2) & 0x3333333333333333ull);
        return static_cast<unsigned>((((x + (x >> 4)) & 0x0F0F0F0F0F0F0F0Full) * 0x0101010101010101ull) >> 56);
#endif
    }
    static inline unsigned ctz64_(std::uint64_t x) noexcept {
#if defined(__cpp_lib_bitops) && __cpp_lib_bitops >= 201907L
        return std::countr_zero(x);
#elif defined(__GNUC__) || defined(__clang__)
        return static_cast<unsigned>(__builtin_ctzll(x));
#else
        unsigned c=0; while ((x & 1ull)==0ull) { x >>= 1; ++c; } return c;
#endif
    }
    static inline unsigned select1_in_word_(std::uint64_t x, unsigned kth) noexcept {
#if (defined(__BMI2__) && (defined(__x86_64__) || defined(_M_X64)))
        std::uint64_t y = _pdep_u64(1ull << kth, x);
        return ctz64_(y);
#else
        std::uint64_t y = x; for (unsigned i = 0; i < kth; ++i) y &= (y - 1); return ctz64_(y);
#endif
    }

    std::size_t select1_in_block_(std::size_t b1, std::uint32_t r_in_block) const noexcept {
        const std::size_t wb = word_base_in_block_(b1);
        std::size_t base = (b1 - 1) * kBlockBits;
        std::uint32_t r = r_in_block;
        for (std::size_t j = 0; j < kWordsPerBlock; ++j) {
            std::uint64_t w = words_[wb + j];
            unsigned c = popcnt64_(w);
            if (r < c) { unsigned bit = select1_in_word_(w, r); return base + j * kWordBits + bit; }
            r -= c;
        }
        return npos;
    }
    std::size_t select0_in_block_(std::size_t b1, std::uint32_t r_in_block) const noexcept {
        const std::size_t wb = word_base_in_block_(b1);
        const std::size_t base = (b1 - 1) * kBlockBits;
        const std::size_t cap = block_capacity_bits_(b1);
        std::uint32_t r = r_in_block;
        for (std::size_t j = 0; j < kWordsPerBlock && j * kWordBits < cap; ++j) {
            std::size_t remain = cap - j * kWordBits;
            std::uint64_t mask = (remain >= 64) ? ~0ull : ((remain == 0) ? 0ull : ((1ull << remain) - 1ull));
            std::uint64_t w = words_[wb + j];
            std::uint64_t zeroes = (~w) & mask;
            unsigned c = popcnt64_(zeroes);
            if (r < c) { unsigned bit = select1_in_word_(zeroes, r); return base + j * kWordBits + bit; }
            r -= c;
        }
        return npos;
    }

    void write_bit_(std::size_t i, bool to) noexcept {
        auto [widx, off] = word_index_(i);
        std::uint64_t mask = 1ull << off;
        bool was = (words_[widx] & mask) != 0;
        if (was == to) return;
        words_[widx] ^= mask;
        const std::size_t b1 = (i / kBlockBits) + 1;
        int delta = to ? +1 : -1;
        total_ones_ += delta;
        fenwick_add_(fenwick_, b1, delta);
    }
    void toggle_bit_(std::size_t i) noexcept {
        auto [widx, off] = word_index_(i);
        std::uint64_t mask = 1ull << off;
        bool was = (words_[widx] & mask) != 0;
        words_[widx] ^= mask;
        const std::size_t b1 = (i / kBlockBits) + 1;
        int delta = was ? -1 : +1;
        total_ones_ += delta;
        fenwick_add_(fenwick_, b1, delta);
    }
};

} // namespace core

