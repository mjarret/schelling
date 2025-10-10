// bitset_tests.cpp
// Focused coverage for plf::bitset search routines.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#define private public
#include "core/plf_bitset.h"
#undef private

#include "core/bitset.hpp"
#include "core/config.hpp"

#include <limits>

namespace {
	template <std::size_t N>
	using test_bitset = core::bitset<N>;

	constexpr std::size_t word_bits = sizeof(CORE_BITSET_WORD_T) * 8;

	template <std::size_t N>
	constexpr std::size_t capacity_words = (N + word_bits - 1) / word_bits;
}

TEST_CASE("search_one_forwards selects ranks across words")
{
	constexpr std::size_t bits = 130;
	test_bitset<bits> bs;
	for(int i = 0; i < 43; ++i) {
		bs.set(i * 3 % bits);
	}

	for(int i = 0; i < 43; ++i) {
		CHECK(bs.search_one_forwards(0, i) == (i * 3 % bits));
	}
}

TEST_CASE("search_zero_forwards enumerates zeros and restores overflow mask")
{
	constexpr std::size_t bits = 130;
	test_bitset<bits> bs;
	for(int i = 0; i < 43; ++i) {
		bs.set(i * 3 % bits);
	}

	bs.flip(); // Invert to test zero search

	for(int i = 0; i < 43; ++i) {
		CHECK(bs.search_zero_forwards(0, i) == (i * 3 % bits));
	}
}

TEST_CASE("search_zero_forwards returns max when fully occupied")
{
	constexpr std::size_t bits = 70;
	test_bitset<bits> bs;
	bs.set();
	CHECK(bs.search_zero_forwards(0, 0) == std::numeric_limits<std::size_t>::max());

	if constexpr ((bits % word_bits) != 0)
	{
		const auto raw_last = bs.data()[capacity_words<bits> - 1];
		const auto overflow = raw_last >> (bits % word_bits);
		CHECK(overflow == 0);
	}
}
