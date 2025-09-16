#pragma once

#include <vector>
#include <cstdint>
#include <limits>
#include <bit>

#if defined(__x86_64__) || defined(_M_X64)
#  include <immintrin.h>
#endif

// Runtime-sized bitset optimized for random 1/0 selection via Fenwick over blocks.
// Stores contiguous 64-bit words; no runtime asserts; branch-lean operations.
class FastBitset {
public:
  static constexpr std::size_t kBlockBits       = 256; // multiple of 64
  static constexpr std::size_t kWordBits        = 64;
  static constexpr std::size_t kWordsPerBlock   = kBlockBits / kWordBits;
  static constexpr std::size_t npos             = std::numeric_limits<std::size_t>::max();

  explicit FastBitset(std::size_t nbits, bool keep_zero_fenwick=false)
  : nbits_(nbits),
    nblocks_((nbits + kBlockBits - 1) / kBlockBits),
    words_(nblocks_ * kWordsPerBlock, 0),
    fenwick_ones_(nblocks_ + 1, 0),
    keep_zero_fenwick_(keep_zero_fenwick),
    fenwick_zeros_(keep_zero_fenwick ? (nblocks_ + 1) : 0, 0),
    total_ones_(0)
  {
    if (keep_zero_fenwick_) {
      for (std::size_t b = 1; b <= nblocks_; ++b) {
        fenwick_add_(fenwick_zeros_, b, static_cast<int>(block_capacity_bits_(b)));
      }
    }
  }

  std::size_t size_bits() const noexcept { return nbits_; }
  std::size_t capacity_bits() const noexcept { return nblocks_ * kBlockBits; }
  std::size_t count() const noexcept { return total_ones_; }

  bool test(std::size_t i) const noexcept {
    if (i >= nbits_) return false;
    auto [w, off] = word_index_(i);
    return (words_[w] >> off) & 1ULL;
  }
  void set(std::size_t i) noexcept { change_bit_(i, true); }
  void reset(std::size_t i) noexcept { change_bit_(i, false); }
  void flip(std::size_t i) noexcept { if (i < nbits_) change_bit_toggle_(i); }

  template<class URBG>
  std::size_t random_one(URBG& rng) const noexcept {
    if (total_ones_ == 0) return npos;
    std::size_t k = rng.uniform_index(total_ones_);
    std::size_t b = fenwick_find_by_order_(fenwick_ones_, static_cast<std::uint32_t>(k + 1));
    std::uint32_t ones_before = fenwick_sum_(fenwick_ones_, b - 1);
    std::uint32_t r_in_block = static_cast<std::uint32_t>(k - ones_before);
    return select_in_block_(b, r_in_block);
  }

  template<class URBG>
  std::size_t random_zero(URBG& rng) const noexcept {
    std::size_t total_zeros = nbits_ - total_ones_;
    if (total_zeros == 0) return npos;
    std::size_t k = rng.uniform_index(total_zeros);
    if (keep_zero_fenwick_) {
      std::size_t b = fenwick_find_by_order_(fenwick_zeros_, static_cast<std::uint32_t>(k + 1));
      std::uint32_t zeros_before = fenwick_sum_(fenwick_zeros_, b - 1);
      std::uint32_t r_in_block = static_cast<std::uint32_t>(k - zeros_before);
      return select_zero_in_block_(b, r_in_block);
    } else {
      std::size_t lo = 1, hi = nblocks_;
      while (lo < hi) {
        std::size_t mid = (lo + hi) >> 1;
        std::uint64_t zeros_mid = prefix_bits_(mid) - fenwick_sum_(fenwick_ones_, mid);
        if (zeros_mid > k) hi = mid; else lo = mid + 1;
      }
      std::uint64_t zeros_before = prefix_bits_(lo - 1) - fenwick_sum_(fenwick_ones_, lo - 1);
      std::uint32_t r_in_block = static_cast<std::uint32_t>(k - zeros_before);
      return select_zero_in_block_(lo, r_in_block);
    }
  }

private:
  std::size_t nbits_;
  std::size_t nblocks_;
  std::vector<std::uint64_t> words_;
  std::vector<std::uint32_t> fenwick_ones_;
  bool keep_zero_fenwick_;
  std::vector<std::uint32_t> fenwick_zeros_;
  std::size_t total_ones_;

  static constexpr std::size_t block_of_bit_(std::size_t i) noexcept { return i / kBlockBits; }
  static constexpr std::size_t word_base_in_block_(std::size_t b1) noexcept { return (b1 - 1) * kWordsPerBlock; }
  static constexpr std::pair<std::size_t, unsigned> word_index_(std::size_t i) noexcept {
    const std::size_t w = (i / kWordBits);
    return {w, static_cast<unsigned>(i & (kWordBits - 1))};
  }
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
  static inline std::uint32_t fenwick_sum_(const std::vector<std::uint32_t>& ft, std::size_t idx) noexcept {
    std::uint32_t s = 0;
    for (std::size_t i = idx; i > 0; i &= (i - 1)) s += ft[i];
    return s;
  }
  static inline std::size_t fenwick_find_by_order_(const std::vector<std::uint32_t>& ft, std::uint32_t k1) noexcept {
    std::size_t n = ft.size() - 1;
    std::size_t idx = 0;
    std::size_t bit = std::size_t(1) << (63 - std::countl_zero(n | 1ull));
    for (; bit; bit >>= 1) {
      std::size_t next = idx + bit;
      if (next <= n && ft[next] < k1) { k1 -= ft[next]; idx = next; }
    }
    return idx + 1;
  }

  static inline unsigned popcnt64_(std::uint64_t x) noexcept {
#if defined(__cpp_lib_bitops) && __cpp_lib_bitops >= 201907L
    return std::popcount(x);
#elif defined(__GNUC__) || defined(__clang__)
    return static_cast<unsigned>(__builtin_popcountll(x));
#else
    x = x - ((x >> 1) & 0x5555555555555555ull);
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
    unsigned c = 0; while ((x & 1ull) == 0ull) { x >>= 1; ++c; } return c;
#endif
  }
  static inline unsigned select1_in_word_(std::uint64_t x, unsigned kth) noexcept {
#if (defined(__BMI2__) || defined(__AVX2__)) && (defined(__x86_64__) || defined(_M_X64))
    std::uint64_t y = _pdep_u64(1ull << kth, x);
    return ctz64_(y);
#else
    std::uint64_t y = x;
    for (unsigned i = 0; i < kth; ++i) y &= (y - 1);
    return ctz64_(y);
#endif
  }

  std::size_t select_in_block_(std::size_t b1, std::uint32_t r_in_block) const noexcept {
    const std::size_t wb = word_base_in_block_(b1);
    std::uint32_t r = r_in_block;
    for (std::size_t j = 0; j < kWordsPerBlock; ++j) {
      std::uint64_t w = words_[wb + j];
      unsigned c = popcnt64_(w);
      if (r < c) {
        unsigned bit = select1_in_word_(w, r);
        return (b1 - 1) * kBlockBits + j * kWordBits + bit;
      }
      r -= c;
    }
    return npos;
  }
  std::size_t select_zero_in_block_(std::size_t b1, std::uint32_t r_in_block) const noexcept {
    const std::size_t wb = word_base_in_block_(b1);
    std::size_t start_bit = (b1 - 1) * kBlockBits;
    std::size_t block_cap = block_capacity_bits_(b1);
    std::uint32_t r = r_in_block;
    for (std::size_t j = 0; j < kWordsPerBlock && j * kWordBits < block_cap; ++j) {
      std::size_t remain = block_cap - j * kWordBits;
      std::uint64_t mask = (remain >= 64) ? ~0ull : ((remain == 0) ? 0ull : ((1ull << remain) - 1ull));
      std::uint64_t w = words_[wb + j];
      std::uint64_t zeroes = (~w) & mask;
      unsigned c = popcnt64_(zeroes);
      if (r < c) {
        unsigned bit = select1_in_word_(zeroes, r);
        return start_bit + j * kWordBits + bit;
      }
      r -= c;
    }
    return npos;
  }

  void change_bit_(std::size_t i, bool set_to) noexcept {
    if (i >= nbits_) return;
    auto [widx, off] = word_index_(i);
    std::uint64_t mask = 1ull << off;
    bool was = (words_[widx] & mask) != 0;
    if (was == set_to) return;
    words_[widx] ^= mask;
    const std::size_t b1 = block_of_bit_(i) + 1;
    int delta = set_to ? +1 : -1;
    total_ones_ += delta;
    fenwick_add_(fenwick_ones_, b1, delta);
    if (keep_zero_fenwick_) fenwick_add_(fenwick_zeros_, b1, -delta);
  }
  void change_bit_toggle_(std::size_t i) noexcept {
    if (i >= nbits_) return;
    auto [widx, off] = word_index_(i);
    std::uint64_t mask = 1ull << off;
    bool was = (words_[widx] & mask) != 0;
    words_[widx] ^= mask;
    const std::size_t b1 = block_of_bit_(i) + 1;
    int delta = was ? -1 : +1;
    total_ones_ += delta;
    fenwick_add_(fenwick_ones_, b1, delta);
    if (keep_zero_fenwick_) fenwick_add_(fenwick_zeros_, b1, -delta);
  }
};

