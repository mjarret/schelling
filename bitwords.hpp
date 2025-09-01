#pragma once
#include <cstdint>

// Can change the default word length below
using DefaultWord = __uint128_t;

// #bits in Word (valid for builtin, absl::uint128, and boost fixed-width)
template <class Word>
constexpr unsigned WORD_BITS = static_cast<unsigned>(sizeof(Word) * 8);

// --- Pointer-based bit packing on contiguous limbs --------------------------
namespace bitpack {

// Mask with 'w' low bits set
template <class Word>
inline Word mask_width(unsigned w) {
    if (w == 0) return Word(0);
    const unsigned WB = WORD_BITS<Word>;
    if (w >= WB) return ~Word(0);
    return (Word(1) << w) - Word(1);
}

// Read 'width' bits at absolute bit offset 'bitpos' from limbs[]
template <class Word>
inline Word get_bits(const Word* limbs, uint64_t bitpos, unsigned width) {
    const unsigned WB = WORD_BITS<Word>;
    const uint64_t widx = bitpos / WB;
    const unsigned  off = static_cast<unsigned>(bitpos % WB);

    if (off + width <= WB) {
        Word m = mask_width<Word>(width);
        return (limbs[widx] >> off) & m;
    } else {
        const unsigned w1 = WB - off;
        const unsigned w2 = width - w1;
        Word low = (limbs[widx] >> off) & mask_width<Word>(w1);
        Word hi  = limbs[widx + 1] & mask_width<Word>(w2);
        return low | (hi << w1);
    }
}

// Write 'width' bits (low bits of value) at absolute bit offset 'bitpos' into limbs[]
template <class Word>
inline void set_bits(Word* limbs, uint64_t bitpos, unsigned width, Word value) {
    const unsigned WB = WORD_BITS<Word>;
    value &= mask_width<Word>(width);
    const uint64_t widx = bitpos / WB;
    const unsigned  off = static_cast<unsigned>(bitpos % WB);

    if (off + width <= WB) {
        Word m = mask_width<Word>(width) << off;
        limbs[widx] = (limbs[widx] & ~m) | (value << off);
    } else {
        const unsigned w1 = WB - off;
        const unsigned w2 = width - w1;
        Word m1 = mask_width<Word>(w1) << off;
        Word m2 = mask_width<Word>(w2);
        limbs[widx]     = (limbs[widx]     & ~m1) | ((value & mask_width<Word>(w1)) << off);
        limbs[widx + 1] = (limbs[widx + 1] & ~m2) | (value >> w1);
    }
}

} // namespace bitpack
