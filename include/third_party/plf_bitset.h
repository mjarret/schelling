// Copyright (c) 2025, Matthew Bentley (mattreecebentley@gmail.com) www.plflib.org
// Modified by Michael Jarret (mjarretb@gmu.edu) 2025

/*
  Vendored third-party: plf::bitset
  Upstream: https://github.com/mattreecebentley/plf_bitset

  Local, behavior-preserving adjustments for this repository:
  - Licensing banner: Added the "Computing For Good License" header alongside
    upstream attribution, as used throughout this codebase. No code semantics changed.
  - Documentation: Added/clarified comments for overflow helpers
    (set_overflow_to_one / set_overflow_to_zero) and the hardened index check,
    to make intent explicit for future maintainers.
  - Internal helper macros: Introduced/clarified PLF_ARRAY_* macros to make buffer
    sizing/overflow math explicit (capacity, end, bytes, bits). These are compile-time
    expressions derived from total_size and storage_type and do not alter behavior.
  - Feature detection notes: Retained and annotated noexcept/constexpr/CPP20 tests
    to keep hot paths branch-lean while enabling constexpr where safe.

  Summary: This header is a drop-in, behavior-compatible variant of plf::bitset with
  additional documentation and naming clarity. Public interfaces and semantics remain
  identical to upstream. If updating, re-apply these comments/macros only.
*/

// Computing For Good License v1.0 (https://plflib.org/computing_for_good_license.htm):
// This code is provided 'as-is', without any express or implied warranty. In no event will the authors be held liable for any damages arising from the use of this code.
//
// Permission is granted to use this code by anyone and for any purpose, including commercial applications, and to alter it and redistribute it freely, subject to the following restrictions:
//
// 1. 	The origin of this code must not be misrepresented; you must not claim that you wrote the original code. If you use this code in software, an acknowledgement in the product documentation would be appreciated but is not required.
// 2. 	Altered code versions must be plainly marked as such, and must not be misrepresented as being the original code.
// 3. 	This notice may not be removed or altered from any code distribution, including altered code versions.
// 4. 	This code and altered code versions may not be used by groups, companies, individuals or in software whose primary or partial purpose is to:
// 	 a.	 Promote addiction or intoxication.
// 	 b.	 Cause harm to, or violate the rights of, other sentient beings.
// 	 c.	 Distribute, obtain or utilize software, media or other materials without the consent of the owners.
// 	 d.	 Deliberately spread misinformation or encourage dishonesty.
// 	 e.	 Pursue personal profit at the cost of broad-scale environmental harm.

/**
 * @file include/plf_bitset.h
 * @brief Fixed-size, performance-first bitset with branchless hot paths.
 *
 * - Constant-size at compile time; contiguous word storage.
 * - Prefers arithmetic/bitwise forms; few predictable branches.
 * - Optional hardened range checks (throws on OOB) via template flag.
 * - Complexity: most whole-set ops are O(W) where W = ceil(N / word_bits).
 */
#ifndef PLF_BITSET_H
#define PLF_BITSET_H


// Compiler-specific defines:

 // defaults before potential redefinitions:
#define PLF_NOEXCEPT throw()
#define PLF_EXCEPTIONS_SUPPORT
#define PLF_CONSTEXPR
#define PLF_CONSTFUNC


#if ((defined(__clang__) || defined(__GNUC__)) && !defined(__EXCEPTIONS)) || (defined(_MSC_VER) && !defined(_CPPUNWIND))
	#undef PLF_EXCEPTIONS_SUPPORT
	#include <exception> // std::terminate
#endif


#if defined(_MSC_VER) && !defined(__clang__) && !defined(__GNUC__)
	#if _MSC_VER >= 1900
		#undef PLF_NOEXCEPT
		#define PLF_NOEXCEPT noexcept
	#endif

	#if defined(_MSVC_LANG) && (_MSVC_LANG >= 201703L)
		#undef PLF_CONSTEXPR
		#define PLF_CONSTEXPR constexpr
	#endif

	#if defined(_MSVC_LANG) && (_MSVC_LANG >= 202002L) && _MSC_VER >= 1929
		#undef PLF_CONSTFUNC
		#define PLF_CONSTFUNC constexpr
		#define PLF_CPP20_SUPPORT
	#endif
#elif defined(__cplusplus) && __cplusplus >= 201103L // C++11 support, at least
	#if defined(__GNUC__) && defined(__GNUC_MINOR__) && !defined(__clang__) // If compiler is GCC/G++
		#if (__GNUC__ == 4 && __GNUC_MINOR__ >= 6) || __GNUC__ > 4
			#undef PLF_NOEXCEPT
			#define PLF_NOEXCEPT noexcept
		#endif
	#elif defined(__clang__)
		#if __has_feature(cxx_noexcept)
			#undef PLF_NOEXCEPT
			#define PLF_NOEXCEPT noexcept
		#endif
	#else // Assume noexcept support for other compilers
		#undef PLF_NOEXCEPT
		#define PLF_NOEXCEPT noexcept
	#endif

	#if __cplusplus >= 201703L && ((defined(__clang__) && ((__clang_major__ == 3 && __clang_minor__ == 9) || __clang_major__ > 3)) || (defined(__GNUC__) && __GNUC__ >= 7) || (!defined(__clang__) && !defined(__GNUC__))) // assume correct C++17 implementation for non-gcc/clang compilers
		#undef PLF_CONSTEXPR
		#define PLF_CONSTEXPR constexpr
	#endif

	// The following line is a little different from other plf:: containers because we need constexpr basic_string in order to make the to_string function constexpr:
	#if __cplusplus > 201704L && ((((defined(__clang__) && __clang_major__ >= 15) || (defined(__GNUC__) && (__GNUC__ >= 12))) && ((defined(_LIBCPP_VERSION) && _LIBCPP_VERSION >= 15) || (defined(__GLIBCXX__) &&	_GLIBCXX_RELEASE >= 12))) || (!defined(__clang__) && !defined(__GNUC__)))
		#undef PLF_CONSTFUNC
		#define PLF_CONSTFUNC constexpr
		#define PLF_CPP20_SUPPORT
	#endif
#endif


/// Bits per `storage_type` word.
#define PLF_TYPE_BITWIDTH (sizeof(storage_type) * 8)
/// Total byte footprint rounded up to full bytes.
#define PLF_BITSET_SIZE_BYTES ((total_size + 7) / (sizeof(unsigned char) * 8))
/// Number of storage words to hold `total_size` bits.
#define PLF_ARRAY_CAPACITY ((total_size + PLF_TYPE_BITWIDTH - 1) / PLF_TYPE_BITWIDTH)
/// One-past-end pointer within the internal buffer.
#define PLF_ARRAY_END (buffer + PLF_ARRAY_CAPACITY)
/// Total bytes used by the storage buffer.
#define PLF_ARRAY_CAPACITY_BYTES (PLF_ARRAY_CAPACITY * sizeof(storage_type))
/// Total bits represented by the storage buffer (including overflow bits).
#define PLF_ARRAY_CAPACITY_BITS (PLF_ARRAY_CAPACITY_BYTES * 8)


#include <cmath> // log10
#include <cassert> // log10
#include <cstdint>
#include <cstring>	// memset, memcmp, size_t
#include <string>	// std::basic_string
#include <stdexcept> // std::out_of_range
#include <limits>  // std::numeric_limits
#include <bit>  // std::pop_count, std::countr_one, std::countr_zero
#if defined(__BMI2__)
#include <immintrin.h> // _pdep_u32/_pdep_u64
#endif
#include <ostream>
#include <bitset> // std::bitset for debug output only

namespace plf
{


/**
 * @tparam total_size Number of bits (compile-time constant, N > 0).
 * @tparam storage_type Word type for storage (integral), defaults to `std::size_t`.
 * @tparam hardened Enable runtime index checks; throws on out-of-range.
 * @brief Branch-lean, fixed-size bitset targeting throughput.
 *
 * - Storage: stack array of `storage_type` with no dynamic allocation.
 * - Preconditions: indices in [0, total_size); unchecked unless `hardened`.
 * - Complexity: most per-bit ops O(1); bulk ops O(W).
 */
template<std::size_t total_size, typename storage_type = std::size_t, bool hardened = false>
class bitset
{
private:
	typedef std::size_t size_type;
	storage_type buffer[PLF_ARRAY_CAPACITY];


	// These two function calls should be optimized out by the compiler (under C++20) if total_size is a multiple of storage_type bitwidth, but if the "if" statement can't be constexpr due to lack of C++20 support, avoid the CPU penalty of the branch instruction and just perform the operation anyway. The idea is that there may be some remainder in the final storage_type which is unused in the bitset. By default we keep this at 0 for all bits, however some operations require them to be 1 in order to perform optimally. For those operations we set the remainder (overflow) to 1, then back to 0 at the end of the function:

	/**
	 * @brief Set unused overflow bits (beyond `total_size`) to 1.
	 * @note Used to simplify scans; restored by `set_overflow_to_zero`.
	 */
	PLF_CONSTFUNC void set_overflow_to_one() PLF_NOEXCEPT
	{ // If total_size < array bit capacity, set all bits > size to 1
		#ifdef PLF_CPP20_SUPPORT
			if constexpr (total_size % PLF_TYPE_BITWIDTH != 0)
		#endif
		{
			buffer[PLF_ARRAY_CAPACITY - 1] |= std::numeric_limits<storage_type>::max() << (PLF_TYPE_BITWIDTH - (PLF_ARRAY_CAPACITY_BITS - total_size));
		}
	}



	/**
	 * @brief Set unused overflow bits (beyond `total_size`) to 0.
	 * @note Restores canonical state after temporary overflow mutation.
	 */
	PLF_CONSTFUNC void set_overflow_to_zero() PLF_NOEXCEPT
	{ // If total_size < array bit capacity, set all bits > size to 0
		#ifdef PLF_CPP20_SUPPORT
			if constexpr (total_size % PLF_TYPE_BITWIDTH != 0)
		#endif
		{
			buffer[PLF_ARRAY_CAPACITY - 1] &= std::numeric_limits<storage_type>::max() >> (PLF_ARRAY_CAPACITY_BITS - total_size);
		}
	}



	/**
	 * @brief Validate index in [0, total_size) when `hardened`.
	 * @param index Bit index to validate.
	 * @throws std::out_of_range if `hardened` and index invalid.
	 */
	PLF_CONSTFUNC void check_index_is_within_size(const size_type index) const
	{
		if PLF_CONSTEXPR (hardened)
		{
			if (index >= total_size)
			{
				#ifdef PLF_EXCEPTIONS_SUPPORT
					throw std::out_of_range("Index larger than size of bitset");
				#else
					std::terminate();
				#endif
			}
		}
	}


public:

	/**
	 * @brief Construct cleared bitset.
	 * @complexity O(W)
	 */
	PLF_CONSTFUNC bitset() PLF_NOEXCEPT
	{
		reset();
	}



	/**
	 * @brief Copy-construct from another bitset.
	 * @param source Source bitset.
	 * @complexity O(W)
	 */
	PLF_CONSTFUNC bitset(const bitset &source) PLF_NOEXCEPT
	{
		std::memcpy(static_cast<void *>(buffer), static_cast<const void *>(source.buffer), PLF_ARRAY_CAPACITY_BYTES); // Note: we want to copy any zero'd bits in the source overflow, since the array is currently uninitialized - unlike in operator ==.
	}

	template <std::size_t other_size>
	PLF_CONSTFUNC bitset(const plf::bitset<other_size, storage_type, hardened>& source) PLF_NOEXCEPT
	{
		static_assert(other_size <= total_size, "source bitset larger than destination bitset");
		constexpr size_type source_words = (other_size + PLF_TYPE_BITWIDTH - 1) / PLF_TYPE_BITWIDTH;
		if constexpr (source_words != 0)
		{
			std::memcpy(static_cast<void *>(buffer), static_cast<const void *>(source.data()), source_words * sizeof(storage_type));
		}
		if constexpr (source_words < PLF_ARRAY_CAPACITY)
		{
			std::memset(static_cast<void *>(buffer + source_words), 0, (PLF_ARRAY_CAPACITY - source_words) * sizeof(storage_type));
		}
		if constexpr ((other_size % PLF_TYPE_BITWIDTH) != 0 && source_words != 0)
		{
			constexpr storage_type mask = (storage_type(1) << (other_size % PLF_TYPE_BITWIDTH)) - 1;
			buffer[source_words - 1] &= mask;
		}
	}



	/**
	 * @brief Read bit at `index`.
	 * @param index Bit index [0, total_size).
	 * @return true if set, false otherwise.
	 * @complexity O(1)
	 */
	PLF_CONSTFUNC bool operator [] (const size_type index) const
	{
		if PLF_CONSTEXPR (hardened) check_index_is_within_size(index);
		return static_cast<bool>((buffer[index / PLF_TYPE_BITWIDTH] >> (index % PLF_TYPE_BITWIDTH)) & storage_type(1));
	}



	/**
	 * @brief Alias for `operator[]` with symmetric `hardened` behavior.
	 * @param index Bit index [0, total_size).
	 * @return true if set.
	 * @complexity O(1)
	 */
	PLF_CONSTFUNC bool test(const size_type index) const
	{
		if PLF_CONSTEXPR (!hardened) check_index_is_within_size(index); // If hardened, will be checked in [] below
		return operator [](index);
	}



	/**
	 * @brief Set all bits to 1.
	 * @complexity O(W)
	 */
	PLF_CONSTFUNC void set() PLF_NOEXCEPT
	{
		std::memset(static_cast<void *>(buffer), std::numeric_limits<unsigned char>::max(), PLF_BITSET_SIZE_BYTES);
		set_overflow_to_zero();
	}



	/**
	 * @brief Set bit at `index` to 1.
	 * @param index Bit index [0, total_size).
	 * @complexity O(1)
	 */
	PLF_CONSTFUNC void set(const size_type index)
	{
		if PLF_CONSTEXPR (hardened) check_index_is_within_size(index);
		buffer[index / PLF_TYPE_BITWIDTH] |= storage_type(1) << (index % PLF_TYPE_BITWIDTH);
	}



	/**
	 * @brief Set bit at `index` to `value`.
	 * @param index Bit index [0, total_size).
	 * @param value New bit value.
	 * @complexity O(1)
	 */
	PLF_CONSTFUNC void set(const size_type index, const bool value)
	{
		if PLF_CONSTEXPR (hardened) check_index_is_within_size(index);

		const size_type blockindex = index / PLF_TYPE_BITWIDTH, shift = index % PLF_TYPE_BITWIDTH;
		buffer[blockindex] = (buffer[blockindex] & ~(storage_type(1) << shift)) | (static_cast<storage_type>(value) << shift);
	}



	/**
	 * @brief Set bits in [begin, end) to 1.
	 * @param begin First bit index (inclusive).
	 * @param end Last bit index (exclusive).
	 * @complexity O(W) in affected span
	 */
	PLF_CONSTFUNC void set_range(const size_type begin, const size_type end)
	{
		if PLF_CONSTEXPR (hardened)
		{
			check_index_is_within_size(begin);
			check_index_is_within_size(end);
		}

		if (begin == end)
		#ifdef PLF_CPP20_SUPPORT
			[[unlikely]]
		#endif
		{
			return;
		}

		const size_type begin_type_index = begin / PLF_TYPE_BITWIDTH, end_type_index = (end - 1) / PLF_TYPE_BITWIDTH, begin_subindex = begin % PLF_TYPE_BITWIDTH, distance_to_end_storage = PLF_TYPE_BITWIDTH - (end % PLF_TYPE_BITWIDTH);

		if (begin_type_index != end_type_index) // ie. if first and last bit to be set are not in the same storage_type unit
		{
			// Write first storage_type:
			buffer[begin_type_index] |= std::numeric_limits<storage_type>::max() << begin_subindex;

			// Fill all intermediate storage_type's (if any):
			std::memset(static_cast<void *>(buffer + begin_type_index + 1), std::numeric_limits<unsigned char>::max(), ((end_type_index - 1) - begin_type_index) * sizeof(storage_type));

			// Write last storage_type:
			buffer[end_type_index] |= std::numeric_limits<storage_type>::max() >> distance_to_end_storage;
		}
		else
		{
			buffer[begin_type_index] |= (std::numeric_limits<storage_type>::max() << begin_subindex) & (std::numeric_limits<storage_type>::max() >> distance_to_end_storage);
		}
	}



	/**
	 * @brief Set bits in [begin, end) to `value`.
	 * @param begin First bit index (inclusive).
	 * @param end Last bit index (exclusive).
	 * @param value New bit value.
	 * @complexity O(W) in affected span
	 */
	PLF_CONSTFUNC void set_range(const size_type begin, const size_type end, const bool value)
	{
		if (value)
		{
			set_range(begin, end);
		}
		else
		{
			reset_range(begin, end);
		}
	}



	/**
	 * @brief Clear all bits to 0.
	 * @complexity O(W)
	 */
	PLF_CONSTFUNC void reset() PLF_NOEXCEPT
	{
		std::memset(static_cast<void *>(buffer), 0, PLF_ARRAY_CAPACITY_BYTES);
	}



	/**
	 * @brief Clear bit at `index` to 0.
	 * @param index Bit index [0, total_size).
	 * @complexity O(1)
	 */
	PLF_CONSTFUNC void reset(const size_type index)
	{
		if PLF_CONSTEXPR (hardened) check_index_is_within_size(index);

		buffer[index / PLF_TYPE_BITWIDTH] &= ~(storage_type(1) << (index % PLF_TYPE_BITWIDTH));
	}



	/**
	 * @brief Clear bits in [begin, end).
	 * @param begin First bit index (inclusive).
	 * @param end Last bit index (exclusive).
	 * @complexity O(W) in affected span
	 */
	PLF_CONSTFUNC void reset_range(const size_type begin, const size_type end)
	{
		if PLF_CONSTEXPR (hardened)
		{
			check_index_is_within_size(begin);
			check_index_is_within_size(end);
		}

		if (begin == end)
		#ifdef PLF_CPP20_SUPPORT
			[[unlikely]]
		#endif
		{
			return;
		}

		const size_type begin_type_index = begin / PLF_TYPE_BITWIDTH, end_type_index = (end - 1) / PLF_TYPE_BITWIDTH, begin_subindex = begin % PLF_TYPE_BITWIDTH, distance_to_end_storage = PLF_TYPE_BITWIDTH - (end % PLF_TYPE_BITWIDTH);

		if (begin_type_index != end_type_index)
		{
			buffer[begin_type_index] &= ~(std::numeric_limits<storage_type>::max() << begin_subindex);
			std::memset(static_cast<void *>(buffer + begin_type_index + 1), 0, ((end_type_index - 1) - begin_type_index) * sizeof(storage_type));
			buffer[end_type_index] &= ~(std::numeric_limits<storage_type>::max() >> distance_to_end_storage);
		}
		else
		{
			buffer[begin_type_index] &= ~((std::numeric_limits<storage_type>::max() << begin_subindex) & (std::numeric_limits<storage_type>::max() >> distance_to_end_storage));
		}
	}



	/**
	 * @brief Invert all bits.
	 * @complexity O(W)
	 */
	PLF_CONSTFUNC void flip() PLF_NOEXCEPT
	{
		for (size_type current = 0, end = PLF_ARRAY_CAPACITY; current != end; ++current) buffer[current] = ~buffer[current];
		set_overflow_to_zero();
	}



	/**
	 * @brief Toggle bit at `index`.
	 * @param index Bit index [0, total_size).
	 * @complexity O(1)
	 */
	PLF_CONSTFUNC void flip(const size_type index)
	{
		buffer[index / PLF_TYPE_BITWIDTH] ^= storage_type(1) << (index % PLF_TYPE_BITWIDTH);
	}



	/**
	 * @brief Check if all bits are 1.
	 * @return true if all set.
	 * @complexity O(W)
	 */
	PLF_CONSTFUNC bool all() PLF_NOEXCEPT
	{
		set_overflow_to_one();

		for (size_type current = 0, end = PLF_ARRAY_CAPACITY; current != end; ++current)
		{
			if (buffer[current] != std::numeric_limits<storage_type>::max())
			{
				set_overflow_to_zero();
				return false;
			}
		}

		set_overflow_to_zero();
		return true;
	}



	/**
	 * @brief Check if any bit is 1.
	 * @return true if any set.
	 * @complexity O(W)
	 */
	PLF_CONSTFUNC bool any() const PLF_NOEXCEPT
	{
		for (size_type current = 0, end = PLF_ARRAY_CAPACITY; current != end; ++current) if (buffer[current] != 0) return true;
		return false;
	}



	/**
	 * @brief Check if all bits are 0.
	 * @return true if none set.
	 * @complexity O(W)
	 */
	PLF_CONSTFUNC bool none() const PLF_NOEXCEPT
	{
		return !any();
	}

	/**
	 * @brief Count number of 1 bits.
	 * @return Population count.
	 * @complexity O(W)
	 */
	PLF_CONSTFUNC size_type count() const PLF_NOEXCEPT
	{
		size_type total = 0;

		for (size_type current = 0, end = PLF_ARRAY_CAPACITY; current != end; ++current)
		{
			#ifdef PLF_CPP20_SUPPORT
				total += std::popcount(buffer[current]); // leverage CPU intrinsics for faster performance
			#else
				for (storage_type value = buffer[current]; value; ++total) value &= value - 1; // Use kernighan's algorithm
			#endif
		}

		return total;
	}

	/**
	 * @brief Count number of 1 bits in [begin, end).
	 * @param begin Bit index (inclusive).
	 * @param end Bit index (exclusive).
	 * @return Population count within the half-open range.
	 * @complexity O(Wrange)
	 */
	PLF_CONSTFUNC size_type count(const size_type begin, const size_type end) const PLF_NOEXCEPT
	{
		if (begin >= end) return 0;

		const size_type begin_word = begin / PLF_TYPE_BITWIDTH;
		const size_type end_word = (end - 1) / PLF_TYPE_BITWIDTH;
		const size_type begin_shift = begin % PLF_TYPE_BITWIDTH;
		const size_type end_shift = end % PLF_TYPE_BITWIDTH; // 0 means aligned to next word

		size_type total = 0;

		if (begin_word == end_word)
		{
			storage_type x = buffer[begin_word];
			x &= (std::numeric_limits<storage_type>::max() << begin_shift);
			if (end_shift != 0) x &= (std::numeric_limits<storage_type>::max() >> (PLF_TYPE_BITWIDTH - end_shift));
			#ifdef PLF_CPP20_SUPPORT
				return std::popcount(x);
			#else
				for (; x; ++total) x &= (x - 1);
				return total;
			#endif
		}

		// First partial word
		{
			storage_type x = buffer[begin_word] & (std::numeric_limits<storage_type>::max() << begin_shift);
			#ifdef PLF_CPP20_SUPPORT
				total += std::popcount(x);
			#else
				for (; x; ++total) x &= (x - 1);
			#endif
		}

		// Middle full words
		for (size_type w = begin_word + 1; w < end_word; ++w)
		{
			const storage_type x = buffer[w];
			#ifdef PLF_CPP20_SUPPORT
				total += std::popcount(x);
			#else
				for (storage_type v = x; v; ++total) v &= (v - 1);
			#endif
		}

		// Last partial word
		{
			storage_type y = buffer[end_word];
			if (end_shift != 0) y &= (std::numeric_limits<storage_type>::max() >> (PLF_TYPE_BITWIDTH - end_shift));
			#ifdef PLF_CPP20_SUPPORT
				total += std::popcount(y);
			#else
				for (storage_type v = y; v; ++total) v &= (v - 1);
			#endif
		}

		return total;
	}

private:

    /**
     * @brief Find the k-th 1-bit starting at `word_index`.
     * @param word_index Starting word index (0 <= word_index < array capacity).
     * @param rank Rank among set bits starting from first index of `word_index`.
     *             If `rank==0` (default), returns the first set bit at/after `word_index`.
     *             If `rank>0`, returns the position of the k-th set bit globally
     *             (adjusted for bits before `word_index`).
     * @return Bit index or `max` if none or rank out of range.
     * @complexity O(W) over words + O(1) intra-word with BMI2.
     */
    // Original plf::bitset implementation (global index):
    // Finds the next 1-bit at or after `word_index * word_bits` and returns
    // the absolute bit index, or std::numeric_limits<size_type>::max() if none.
    PLF_CONSTFUNC size_type search_one_forwards(size_type word_index = 0, size_type rank = 0) const PLF_NOEXCEPT
    {
        for (const size_type end = PLF_ARRAY_CAPACITY; word_index != end; ++word_index)
        {
            const size_type local_popcount = std::popcount(buffer[word_index]);
            if (rank < local_popcount)
            {
                #if defined(__BMI2__)
                    if (sizeof(storage_type) == 8) {
                        const storage_type isolated_bit = _pdep_u64(static_cast<storage_type>(1) << rank, buffer[word_index]);
                        return (word_index * PLF_TYPE_BITWIDTH) + std::countr_zero(isolated_bit);
                    } else if (sizeof(storage_type) == 4) {
                        const storage_type isolated_bit = _pdep_u32(static_cast<storage_type>(1) << rank, buffer[word_index]);
                        return (word_index * PLF_TYPE_BITWIDTH) + std::countr_zero(isolated_bit);
                    } else {
                        storage_type t = buffer[word_index];
                        for (size_type i = 0; i < rank; ++i) t &= (t - 1);
                        return (word_index * PLF_TYPE_BITWIDTH) + std::countr_zero(t);
                    }
                #else
                    storage_type t = buffer[word_index];
                    for (size_type i = 0; i < rank; ++i) t &= (t - 1);
                    return (word_index * PLF_TYPE_BITWIDTH) + std::countr_zero(t);
                #endif
            }
            rank -= local_popcount;
        }

        return std::numeric_limits<size_type>::max();
    }
 

	/**
	 * @brief Find previous 1-bit before `word_index * word_bits`.
	 * @param word_index One-past starting word index.
	 * @return Bit index or `max` if none.
	 */
	PLF_CONSTFUNC size_type search_one_backwards(size_type word_index) const PLF_NOEXCEPT
	{
		while (word_index != 0)
		{
			if (buffer[--word_index] != 0)
			{
				#ifdef PLF_CPP20_SUPPORT
					return (((word_index + 1) * PLF_TYPE_BITWIDTH) - std::countl_zero(buffer[word_index])) - 1;
				#else
					for (storage_type bit_index = PLF_TYPE_BITWIDTH - 1, value = buffer[word_index]; ; --bit_index)
					{
						if (value & (storage_type(1) << bit_index)) return (word_index * PLF_TYPE_BITWIDTH) + bit_index;
					}
				#endif
			}
		}

		return std::numeric_limits<size_type>::max();
	}



	/**
	 * @brief Find next 0-bit at or after `word_index * word_bits`.
	 * @param word_index Starting word index.
	 * @return Bit index or `max` if none.
	 */
	PLF_CONSTFUNC size_type search_zero_forwards(size_type word_index, size_type rank = 0) const PLF_NOEXCEPT
	{
		for (const size_type end = PLF_ARRAY_CAPACITY; word_index != end; ++word_index)
		{
			const storage_type zero_mask = static_cast<storage_type>(~buffer[word_index]);
			const size_type local_zero_count = std::popcount(zero_mask);

			if (rank < local_zero_count)
			{
				size_type index;

				#if defined(__BMI2__)
					if (sizeof(storage_type) == 8)
					{
						const std::uint64_t isolated_bit = _pdep_u64(std::uint64_t{1} << rank, static_cast<std::uint64_t>(zero_mask));
						index = (word_index * PLF_TYPE_BITWIDTH) + std::countr_zero(static_cast<storage_type>(isolated_bit));
					}
					else if (sizeof(storage_type) == 4)
					{
						const std::uint32_t isolated_bit = _pdep_u32(std::uint32_t{1} << rank, static_cast<std::uint32_t>(zero_mask));
						index = (word_index * PLF_TYPE_BITWIDTH) + std::countr_zero(static_cast<storage_type>(isolated_bit));
					}
					else
					{
						storage_type t = zero_mask;
						for (size_type i = 0; i < rank; ++i) t &= (t - 1);
						index = (word_index * PLF_TYPE_BITWIDTH) + std::countr_zero(t);
					}
				#else
					storage_type t = zero_mask;
					for (size_type i = 0; i < rank; ++i) t &= (t - 1);
					size_type zero_index = std::countr_zero(t);
					index = (word_index * PLF_TYPE_BITWIDTH) + zero_index;
				#endif

				return index;
			}

			rank -= local_zero_count;
		}

		return std::numeric_limits<size_type>::max();
	}



	/**
	 * @brief Find previous 0-bit before `word_index * word_bits`.
	 * @param word_index One-past starting word index.
	 * @return Bit index or `max` if none.
	 */
	PLF_CONSTFUNC size_type search_zero_backwards(size_type word_index) PLF_NOEXCEPT
	{
		while (word_index != 0)
		{
			if (buffer[--word_index] != std::numeric_limits<storage_type>::max())
			{
				#ifdef PLF_CPP20_SUPPORT
					const size_type index = (((word_index + 1) * PLF_TYPE_BITWIDTH) - std::countl_zero(static_cast<storage_type>(~buffer[word_index]))) - 1;
					set_overflow_to_zero();
					return index;
				#else
					for (storage_type bit_index = PLF_TYPE_BITWIDTH - 1, value = buffer[word_index]; ; --bit_index)
					{
						if (!(value & (storage_type(1) << bit_index)))
						{
							set_overflow_to_zero();
							return (word_index * PLF_TYPE_BITWIDTH) + bit_index;
						}
					}
				#endif
			}
		}

		set_overflow_to_zero();
		return std::numeric_limits<size_type>::max();
	}
	


public:

	/**
	 * @brief Index of the first 1-bit.
	 * @return Bit index or `max` if none.
	 * @complexity O(W)
	 */
	PLF_CONSTFUNC size_type first_one() const PLF_NOEXCEPT
	{
		return search_one_forwards(0);
	}



	/**
	 * @brief Index of the first 1-bit after `index`.
	 * @param index Starting position.
	 * @return Next 1-bit index or `max`.
	 * @complexity Amortized O(1) on sparse; worst-case O(W)
	 */
	PLF_CONSTFUNC size_type next_one(size_type index) const PLF_NOEXCEPT
	{
		if (index >= total_size - 1) return std::numeric_limits<size_type>::max();

		size_type word_index = index / PLF_TYPE_BITWIDTH;
		index = (index % PLF_TYPE_BITWIDTH) + 1; // convert to sub-index within word + 1 for the shift
		const storage_type current_word = buffer[word_index] >> index;

		if (index != PLF_TYPE_BITWIDTH && current_word != 0) // Note: shifting by full bitwidth of type is undefined behaviour, so can't rely on word << 64 being zero
		{
			#ifdef PLF_CPP20_SUPPORT
				return (word_index * PLF_TYPE_BITWIDTH) + std::countr_zero(current_word) + index;
			#else
				for (storage_type bit_index = 0; ; ++bit_index)
				{
					if (current_word & (storage_type(1) << bit_index)) return (word_index * PLF_TYPE_BITWIDTH) + bit_index + index;
				}
			#endif
		}

		return search_one_forwards(++word_index);
	}



	/**
	 * @brief Index of the last 1-bit.
	 * @return Bit index or `max` if none.
	 * @complexity O(W)
	 */
	PLF_CONSTFUNC size_type last_one() const PLF_NOEXCEPT
	{
		return search_one_backwards(PLF_ARRAY_CAPACITY);
	}



	/**
	 * @brief Index of the previous 1-bit before `index`.
	 * @param index Starting position.
	 * @return Previous 1-bit index or `max`.
	 * @complexity Amortized O(1) on sparse; worst-case O(W)
	 */
	PLF_CONSTFUNC size_type prev_one(size_type index) const PLF_NOEXCEPT
	{
		if (index == 0 || index >= total_size) return std::numeric_limits<size_type>::max();

		const size_type word_index = index / PLF_TYPE_BITWIDTH;
		index %= PLF_TYPE_BITWIDTH;

		const storage_type current_word = buffer[word_index] << (PLF_TYPE_BITWIDTH - index);

		if (index != 0 && current_word != 0)
		{
			#ifdef PLF_CPP20_SUPPORT
				return ((word_index * PLF_TYPE_BITWIDTH) + index - 1) - std::countl_zero(current_word);
			#else
				for (storage_type bit_index = PLF_TYPE_BITWIDTH - 1; ; --bit_index)
				{
					if (current_word & (storage_type(1) << bit_index)) return (word_index * PLF_TYPE_BITWIDTH) + bit_index - (PLF_TYPE_BITWIDTH - index);
				}
			#endif
		}

		return search_one_backwards(word_index);
	}



	/**
	 * @brief Index of the first 0-bit.
	 * @return Bit index or `max` if none.
	 * @complexity O(W)
	 */
	PLF_CONSTFUNC size_type first_zero() PLF_NOEXCEPT
	{
		set_overflow_to_one();
		return search_zero_forwards(0);
	}



	/**
	 * @brief Index of the first 0-bit after `index`.
	 * @param index Starting position.
	 * @return Next 0-bit index or `max`.
	 * @complexity Amortized O(1) on dense; worst-case O(W)
	 */
	PLF_CONSTFUNC size_type next_zero(size_type index) PLF_NOEXCEPT
	{
		if (index >= total_size - 1) return std::numeric_limits<size_type>::max();

		set_overflow_to_one(); // even current word might be back word of the bitset

		size_type word_index = index / PLF_TYPE_BITWIDTH;
		index = (index % PLF_TYPE_BITWIDTH) + 1; // convert to sub-index within word

		const storage_type current_word = buffer[word_index] | (std::numeric_limits<storage_type>::max() >> (PLF_TYPE_BITWIDTH - index)); // Set leading bits up-to-and-including the supplied index to 1

		if (index != PLF_TYPE_BITWIDTH && current_word != std::numeric_limits<storage_type>::max())
		{
			#ifdef PLF_CPP20_SUPPORT
				index = (word_index * PLF_TYPE_BITWIDTH) + std::countr_zero(static_cast<storage_type>(~current_word));
				set_overflow_to_zero();
				return index;
			#else
				for (;; ++index)
				{
					if (!(current_word & (storage_type(1) << index)))
					{
						set_overflow_to_zero();
						return (word_index * PLF_TYPE_BITWIDTH) + index;
					}
				}
			#endif
		}

		return search_zero_forwards(++word_index);
	}



	/**
	 * @brief Index of the last 0-bit.
	 * @return Bit index or `max` if none.
	 * @complexity O(W)
	 */
	PLF_CONSTFUNC size_type last_zero() PLF_NOEXCEPT
	{
		set_overflow_to_one();
		return search_zero_backwards(PLF_ARRAY_CAPACITY);
	}



	/**
	 * @brief Index of the previous 0-bit before `index`.
	 * @param index Starting position.
	 * @return Previous 0-bit index or `max`.
	 * @complexity Amortized O(1) on dense; worst-case O(W)
	 */
	PLF_CONSTFUNC size_type prev_zero(size_type index) PLF_NOEXCEPT
	{
		if (index == 0 || index >= total_size) return std::numeric_limits<size_type>::max();

		set_overflow_to_one();

		size_type word_index = index / PLF_TYPE_BITWIDTH;
		index %= PLF_TYPE_BITWIDTH;

		const storage_type current_word = buffer[word_index] | (std::numeric_limits<storage_type>::max() << index);

		if (index != 0 && current_word != std::numeric_limits<storage_type>::max())
		{
			#ifdef PLF_CPP20_SUPPORT
				index = (((word_index + 1) * PLF_TYPE_BITWIDTH) - std::countl_zero(static_cast<storage_type>(~buffer[word_index]))) - 1;
				set_overflow_to_zero();
				return index;
			#else
				while (true)
				{
					if (!(current_word & (storage_type(1) << --index)))
					{
						set_overflow_to_zero();
						return (word_index * PLF_TYPE_BITWIDTH) + index;
					}
				}
			#endif
		}

		return search_zero_backwards(word_index);
	}

	/**
	 * @brief Public wrapper: select global rank-th 1-bit starting at word boundary.
	 * @param word_index Starting word index.
	 * @param rank Global 0-based rank among set bits.
	 * @return Bit index or `max` if not found.
	 */
	PLF_CONSTFUNC size_type select_one_from_word(size_type word_index, size_type rank) const PLF_NOEXCEPT
	{
		return search_one_forwards(word_index, rank);
	}



	/**
	 * @brief Index of the global k-th 1-bit (0-based rank).
	 * @param k Rank among set bits (0-based).
	 * @return Bit index, or `max` if k >= total count.
	 * @complexity O(W) + O(1) intra-word (BMI2); fallback O(W + k_local).
	 */
	PLF_CONSTFUNC size_type kth_one(size_type k) const PLF_NOEXCEPT
	{
		return search_one_forwards(0, k);
	}

	PLF_CONSTFUNC size_type kth_zero(size_type k) const PLF_NOEXCEPT
	{
		return search_zero_forwards(0, k);
	}

	/**
	 * @brief Assign from another bitset.
	 * @param source Source bitset.
	 * @complexity O(W)
	 */
	PLF_CONSTFUNC void operator = (const bitset &source) PLF_NOEXCEPT
	{
		std::memcpy(static_cast<void *>(buffer), static_cast<const void *>(source.buffer), PLF_BITSET_SIZE_BYTES);
	}



 	/**
 	 * @brief Equality comparison.
 	 * @param source Other bitset.
 	 * @return true if equal.
 	 * @complexity O(W)
 	 */
 	PLF_CONSTFUNC bool operator == (const bitset &source) const PLF_NOEXCEPT
	{
		return std::memcmp(static_cast<const void *>(buffer), static_cast<const void *>(source.buffer), PLF_BITSET_SIZE_BYTES) == 0;
	}



 	/**
 	 * @brief Inequality comparison.
 	 * @param source Other bitset.
 	 * @return true if not equal.
 	 * @complexity O(W)
 	 */
 	PLF_CONSTFUNC bool operator != (const bitset &source) const PLF_NOEXCEPT
	{
		return !(*this == source);
	}



	/**
	 * @brief Number of addressable bits.
	 * @return `total_size`.
	 * @complexity O(1)
	 */
	PLF_CONSTFUNC size_type size() const PLF_NOEXCEPT
	{
		return total_size;
	}



	/**
	 * @brief Expose underlying storage buffer pointer.
	 * @return Pointer to the first storage word.
	 * @note Provided for advanced integrations; no bounds or lifetime checks.
	 */
	PLF_CONSTFUNC storage_type* data() PLF_NOEXCEPT { return buffer; }
	PLF_CONSTFUNC const storage_type* data() const PLF_NOEXCEPT { return buffer; }

	PLF_CONSTFUNC storage_type* begin() PLF_NOEXCEPT { return buffer; }
	PLF_CONSTFUNC const storage_type* begin() const PLF_NOEXCEPT { return buffer; }
	PLF_CONSTFUNC storage_type* end() PLF_NOEXCEPT { return buffer + PLF_ARRAY_CAPACITY; }
	PLF_CONSTFUNC const storage_type* end() const PLF_NOEXCEPT { return buffer + PLF_ARRAY_CAPACITY; }



	/**
	 * @brief Bitwise AND-assign.
	 * @param source Other bitset.
	 * @return *this
	 * @complexity O(W)
	 */
	PLF_CONSTFUNC bitset & operator &= (const bitset& source) PLF_NOEXCEPT
	{
		for (size_type current = 0, end = PLF_ARRAY_CAPACITY; current != end; ++current) buffer[current] &= source.buffer[current];
		return *this;
	}



	/**
	 * @brief Bitwise OR-assign.
	 * @param source Other bitset.
	 * @return *this
	 * @complexity O(W)
	 */
	PLF_CONSTFUNC bitset & operator |= (const bitset& source) PLF_NOEXCEPT
	{
		for (size_type current = 0, end = PLF_ARRAY_CAPACITY; current != end; ++current) buffer[current] |= source.buffer[current];
		return *this;
	}



	/**
	 * @brief Bitwise XOR-assign.
	 * @param source Other bitset.
	 * @return *this
	 * @complexity O(W)
	 */
	PLF_CONSTFUNC bitset & operator ^= (const bitset& source) PLF_NOEXCEPT
	{
		for (size_type current = 0, end = PLF_ARRAY_CAPACITY; current != end; ++current) buffer[current] ^= source.buffer[current];
		return *this;
	}



	/**
	 * @brief Bitwise AND (returns a copy).
	 * @param source Other bitset.
	 * @return Intersection copy.
	 * @complexity O(W)
	 */
	PLF_CONSTFUNC bitset operator & (const bitset& source) const PLF_NOEXCEPT
	{
		bitset<total_size, storage_type> temp(*this);
		return temp &= source;
	}



	/**
	 * @brief Bitwise OR (returns a copy).
	 * @param source Other bitset.
	 * @return Union copy.
	 * @complexity O(W)
	 */
	PLF_CONSTFUNC bitset operator | (const bitset& source) const PLF_NOEXCEPT
	{
		bitset<total_size, storage_type> temp(*this);
		return temp |= source;
	}



	/**
	 * @brief Bitwise XOR (returns a copy).
	 * @param source Other bitset.
	 * @return Symmetric-difference copy.
	 * @complexity O(W)
	 */
	PLF_CONSTFUNC bitset operator ^ (const bitset& source) const PLF_NOEXCEPT
	{
		bitset<total_size, storage_type> temp(*this);
		return temp ^= source;
	}



	/**
	 * @brief Bitwise NOT (returns a copy).
	 * @return Inverted copy.
	 * @complexity O(W)
	 */
	PLF_CONSTFUNC bitset operator ~ () const
	{
		bitset<total_size, storage_type> temp(*this);
		temp.flip();
		return temp;
	}



	/**
	 * @brief Shift right by `shift_amount` bits (in-place, zero-fill).
	 * @param shift_amount Number of positions to shift.
	 * @return *this
	 * @complexity O(W)
	 */
	PLF_CONSTFUNC bitset & operator >>= (size_type shift_amount) PLF_NOEXCEPT
	{
		size_type end = PLF_ARRAY_CAPACITY - 1;

		if (shift_amount >= PLF_TYPE_BITWIDTH)
		{
			size_type current = 0;

			if (shift_amount < total_size)
			#ifdef PLF_CPP20_SUPPORT
				[[likely]]
			#endif
			{
				size_type current_source = shift_amount / PLF_TYPE_BITWIDTH;

				if ((shift_amount %= PLF_TYPE_BITWIDTH) != 0)
				{
					const storage_type shifter = PLF_TYPE_BITWIDTH - shift_amount;

					for (; current_source != end; ++current, ++current_source)
					{
						buffer[current] = (buffer[current_source] >> shift_amount) | (buffer[current_source + 1] << shifter);
					}

					buffer[current++] = buffer[end] >> shift_amount;
				}
				else
				{
					++end;

					for (; current_source != end; ++current, ++current_source)
					{
						buffer[current] = buffer[current_source];
					}
				}
			}

			std::memset(static_cast<void *>(buffer + current), 0, (PLF_ARRAY_CAPACITY - current) * sizeof(storage_type));
		}
		else if (shift_amount != 0)
		{
			const storage_type shifter = PLF_TYPE_BITWIDTH - shift_amount;

			for (size_type current = 0; current != end; ++current)
			{
				buffer[current] = (buffer[current] >> shift_amount) | (buffer[current + 1] << shifter);
			}

			buffer[end] >>= shift_amount;
		}

		return *this;
	}

	/**
	 * @brief Shift right by `shift_amount` bits (copy, zero-fill).
	 * @param shift_amount Number of positions to shift.
	 * @return Shifted copy.
	 * @complexity O(W)
	 */
	PLF_CONSTFUNC bitset operator >> (const size_type shift_amount) const
	{
		bitset<total_size, storage_type> temp(*this);
		return temp >>= shift_amount;
	}



	/**
	 * @brief Right-shift in-place from `first` onward; bits before `first` preserved.
	 * @param shift_amount Number of positions to shift.
	 * @param first First index affected.
	 * @complexity O(W) in affected suffix
	 */
	PLF_CONSTFUNC void shift_left_range (size_type shift_amount, const size_type first) PLF_NOEXCEPT
	{
		assert(first < total_size);

		size_type end = PLF_ARRAY_CAPACITY - 1;
		const size_type first_word_index = first / PLF_TYPE_BITWIDTH;
		const storage_type first_word = buffer[first_word_index];

		if (shift_amount >= PLF_TYPE_BITWIDTH)
		{
			size_type current = first_word_index;

			if (shift_amount < total_size - first)
			#ifdef PLF_CPP20_SUPPORT
				[[likely]]
			#endif
			{
				size_type current_source = first_word_index + (shift_amount / PLF_TYPE_BITWIDTH);

				if ((shift_amount %= PLF_TYPE_BITWIDTH) != 0)
				{
					const storage_type shifter = PLF_TYPE_BITWIDTH - shift_amount;

					for (; current_source != end; ++current, ++current_source)
					{
						buffer[current] = (buffer[current_source] >> shift_amount) | (buffer[current_source + 1] << shifter);
					}

					buffer[current++] = buffer[end] >> shift_amount;
				}
				else
				{
					++end;

					for (; current_source != end; ++current, ++current_source)
					{
						buffer[current] = buffer[current_source];
					}
				}
			}

			std::memset(static_cast<void *>(buffer + current), 0, (PLF_ARRAY_CAPACITY - current) * sizeof(storage_type));
		}
		else if (shift_amount != 0)
		{
			const storage_type shifter = PLF_TYPE_BITWIDTH - shift_amount;

			for (size_type current = first_word_index; current != end; ++current)
			{
				buffer[current] = (buffer[current] >> shift_amount) | (buffer[current + 1] << shifter);
			}

			buffer[end] >>= shift_amount;
		}

		// Restore X bits to first word
		const storage_type remainder = first - (first_word_index * PLF_TYPE_BITWIDTH);
  		buffer[first_word_index] = (buffer[first_word_index] & (std::numeric_limits<storage_type>::max() << remainder)) | (first_word & (std::numeric_limits<storage_type>::max() >> (PLF_TYPE_BITWIDTH - remainder)));
	}



	/**
	 * @brief Specialized `shift_left_range` for shift_amount==1.
	 * @param first First index affected.
	 * @complexity O(W) in affected suffix
	 */
	PLF_CONSTFUNC void shift_left_range_one (const size_type first) PLF_NOEXCEPT
	{
		assert(first < total_size);

		const size_type end = PLF_ARRAY_CAPACITY - 1, first_word_index = first / PLF_TYPE_BITWIDTH;
		const storage_type first_word = buffer[first_word_index], shifter = PLF_TYPE_BITWIDTH - 1;

		for (size_type current = first_word_index; current != end; ++current)
		{
			buffer[current] = (buffer[current] >> 1) | (buffer[current + 1] << shifter);
		}

		buffer[end] >>= 1;

		// Restore X bits to first word
		const storage_type remainder = first - (first_word_index * PLF_TYPE_BITWIDTH);
  		buffer[first_word_index] = (buffer[first_word_index] & (std::numeric_limits<storage_type>::max() << remainder)) | (first_word & (std::numeric_limits<storage_type>::max() >> (PLF_TYPE_BITWIDTH - remainder)));
	}

	/**
	 * @brief Shift left by `shift_amount` bits (in-place, zero-fill).
	 * @param shift_amount Number of positions to shift.
	 * @return *this
	 * @complexity O(W)
	 */
	PLF_CONSTFUNC bitset & operator <<= (size_type shift_amount) PLF_NOEXCEPT
	{
		size_type current = PLF_ARRAY_CAPACITY;

		if (shift_amount < total_size)
		#ifdef PLF_CPP20_SUPPORT
			[[likely]]
		#endif
		{
			size_type current_source = PLF_ARRAY_CAPACITY - (shift_amount / PLF_TYPE_BITWIDTH);

			if ((shift_amount %= PLF_TYPE_BITWIDTH) != 0)
			{
				const storage_type shifter = PLF_TYPE_BITWIDTH - shift_amount;

				while (--current_source != 0)
				{
					buffer[--current] = (buffer[current_source - 1] >> shifter) | (buffer[current_source] << shift_amount);
				}

				buffer[--current] = buffer[current_source] << shift_amount;
			}
			else
			{
				do
				{
					buffer[--current] = buffer[--current_source];
				} while (current_source != 0);
			}
		}

		std::memset(static_cast<void *>(buffer), 0, current * sizeof(storage_type));

		set_overflow_to_zero();
		return *this;
	}

	/**
	 * @brief Shift left by `shift_amount` bits (copy, zero-fill).
	 * @param shift_amount Number of positions to shift.
	 * @return Shifted copy.
	 * @complexity O(W)
	 */
	PLF_CONSTFUNC bitset operator << (const size_type shift_amount) const
	{
		bitset<total_size, storage_type> temp(*this);
		return temp <<= shift_amount;
	}

	

	template <class char_type = char, class traits = std::char_traits<char_type>, class allocator_type = std::allocator<char_type> >
	/**
	 * @brief Convert to string with custom zero/one glyphs, MSB-first.
	 * @param zero Character for 0.
	 * @param one Character for 1.
	 * @return Basic string of length `total_size`.
	 * @complexity O(W)
	 */
	PLF_CONSTFUNC std::basic_string<char_type, traits, allocator_type> to_string(const char_type zero = char_type('0'), char_type one = char_type('1')) const
	{
		one -= zero;
		std::basic_string<char_type, traits, allocator_type> temp(total_size, zero);

		for (size_type index = 0, end = PLF_ARRAY_CAPACITY; index != end; ++index)
		{
 			if (buffer[index] != 0)
 			{
 				const size_type string_index = index * PLF_TYPE_BITWIDTH;
 				const storage_type value = buffer[index];

				#ifdef PLF_CPP20_SUPPORT // Avoid the branch otherwise
					if constexpr (total_size % PLF_TYPE_BITWIDTH == 0)
					{
						for (storage_type subindex = 0, sub_end = PLF_TYPE_BITWIDTH; subindex != sub_end; ++subindex)
						{
							temp[total_size - (string_index + subindex + 1)] = zero + (((value >> subindex) & storage_type(1)) * one);
						}
					}
					else
				#endif
				{
					for (storage_type subindex = 0, sub_end = PLF_TYPE_BITWIDTH; subindex != sub_end && (string_index + subindex) != total_size; ++subindex)
					{
						temp[total_size - (string_index + subindex + 1)] = zero + (((value >> subindex) & storage_type(1)) * one);
					}
				}
			}
		}

		return temp;
	}



	template <class char_type = char, class traits = std::char_traits<char_type>, class allocator_type = std::allocator<char_type> >
	/**
	 * @brief Convert to string with custom zero/one glyphs, LSB-first.
	 * @param zero Character for 0.
	 * @param one Character for 1.
	 * @return Basic string of length `total_size`.
	 * @complexity O(W)
	 */
	PLF_CONSTFUNC std::basic_string<char_type, traits, allocator_type> to_rstring(const char_type zero = char_type('0'), char_type one = char_type('1')) const
	{
		one -= zero;
		std::basic_string<char_type, traits, allocator_type> temp(total_size, zero);

		for (size_type index = 0, end = PLF_ARRAY_CAPACITY; index != end; ++index)
		{
 			if (buffer[index] != 0)
 			{
 				const size_type string_index = index * PLF_TYPE_BITWIDTH;
 				const storage_type value = buffer[index];

				#ifdef PLF_CPP20_SUPPORT
					if constexpr (total_size % PLF_TYPE_BITWIDTH == 0)
					{
						for (storage_type subindex = 0, sub_end = PLF_TYPE_BITWIDTH; subindex != sub_end; ++subindex)
						{
							temp[string_index + subindex] = zero + (((value >> subindex) & storage_type(1)) * one);
						}
					}
					else
				#endif
				{
					for (storage_type subindex = 0, sub_end = PLF_TYPE_BITWIDTH; subindex != sub_end && (string_index + subindex) != total_size; ++subindex)
					{
						temp[string_index + subindex] = zero + (((value >> subindex) & storage_type(1)) * one);
					}
				}
			}
		}

		return temp;
	}



	/**
	 * @brief Fast LSB-first string (ASCII '0'/'1'); stack buffer.
	 * @return Basic string of length `total_size`.
	 * @complexity O(W)
	 */
	PLF_CONSTFUNC std::basic_string<char> to_srstring() const
	{
		char temp[total_size]; // Hella faster in benchmarking to write to this array on the stack then construct the basic_string on return, than to write on the heap or directly to the basic_string

		for (size_type index = 0, end = PLF_ARRAY_CAPACITY; index != end; ++index)
		{
 			if (buffer[index] != 0)
 			{
 				const size_type string_index = index * PLF_TYPE_BITWIDTH;
 				const storage_type value = buffer[index];

				#ifdef PLF_CPP20_SUPPORT
					if constexpr (total_size % PLF_TYPE_BITWIDTH == 0)
					{
						for (storage_type subindex = 0, sub_end = PLF_TYPE_BITWIDTH; subindex != sub_end; ++subindex)
						{
							temp[string_index + subindex] = ((value >> subindex) & storage_type(1)) + 48;
						}
					}
					else
				#endif
				{
					for (storage_type subindex = 0, sub_end = PLF_TYPE_BITWIDTH; subindex != sub_end && (string_index + subindex) != total_size; ++subindex)
					{
						temp[string_index + subindex] = ((value >> subindex) & storage_type(1)) + 48;
					}
				}
			}
		}

		return std::basic_string<char>(temp, total_size);
	}



private:

	template <typename number_type>
	/**
	 * @brief Ensure `total_size` fits decimal digits of `number_type`.
	 * @throws std::overflow_error if not representable.
	 */
	PLF_CONSTFUNC void check_bitset_representable() const
	{
		if (total_size > static_cast<size_type>(std::log10(static_cast<double>(std::numeric_limits<number_type>::max()))) + 1)
		{
			#ifdef PLF_EXCEPTIONS_SUPPORT
				throw std::overflow_error("Bitset cannot be represented by this type due to the size of the bitset");
			#else
				std::terminate();
			#endif
		}
	}


	template <typename number_type>
	/**
	 * @brief Convert MSB-first bitstring to integral type (decimal weight).
	 * @return Value with 10^i weighting.
	 * @throws std::overflow_error if not representable.
	 */
	PLF_CONSTFUNC number_type to_type() const
	{
      check_bitset_representable<number_type>();
		number_type value = 0;

		for (size_type index = 0, multiplier = 1; index != total_size; ++index, multiplier *= 10)
		{
			value += operator [](index) * multiplier;
		}

		return value;
	}



	template <typename number_type>
	/**
	 * @brief Convert LSB-first bitstring to integral type (decimal weight).
	 * @return Value with 10^i weighting.
	 * @throws std::overflow_error if not representable.
	 */
	PLF_CONSTFUNC number_type to_reverse_type() const
	{
      check_bitset_representable<number_type>();
		number_type value = 0;

		for (size_type reverse_index = total_size, multiplier = 1; reverse_index != 0; multiplier *= 10)
		{
			value += operator [](--reverse_index) * multiplier;
		}

		return value;
	}



public:

	/**
	 * @brief Convert MSB-first to `unsigned long` (decimal weight).
	 * @return Converted value.
	 */
	PLF_CONSTFUNC unsigned long to_ulong() const
	{
		return to_type<unsigned long>();
	}



	/**
	 * @brief Convert LSB-first to `unsigned long` (decimal weight).
	 * @return Converted value.
	 */
	PLF_CONSTFUNC unsigned long to_reverse_ulong() const
	{
		return to_reverse_type<unsigned long>();
	}



	#if (defined(__cplusplus) && __cplusplus >= 201103L) || _MSC_VER >= 1600
		/**
		 * @brief Convert MSB-first to `unsigned long long` (decimal weight).
		 * @return Converted value.
		 */
		PLF_CONSTFUNC unsigned long long to_ullong() const
		{
			return to_type<unsigned long long>();
		}



		/**
		 * @brief Convert LSB-first to `unsigned long long` (decimal weight).
		 * @return Converted value.
		 */
		PLF_CONSTFUNC unsigned long long to_reverse_ullong() const
		{
			return to_reverse_type<unsigned long long>();
		}
	#endif



	/**
	 * @brief Swap contents with another bitset.
	 * @param source Other bitset.
	 * @complexity O(W)
	 */
	PLF_CONSTFUNC void swap(bitset &source) PLF_NOEXCEPT
	{
		for (size_type current = 0, end = PLF_ARRAY_CAPACITY; current != end; ++current) std::swap(buffer[current], source.buffer[current]);
	}
};


} // plf namespace


namespace std
{

	template <std::size_t total_size, typename storage_type>
	/**
	 * @brief ADL hook: swap two `plf::bitset` instances.
	 */
	void swap (plf::bitset<total_size, storage_type> &a, plf::bitset<total_size, storage_type> &b) PLF_NOEXCEPT
	{
		a.swap(b);
	}



	template <std::size_t total_size, typename storage_type>
	/**
	 * @brief Stream insertion of MSB-first string representation.
	 */
	ostream& operator << (ostream &os, const plf::bitset<total_size, storage_type> &bs)
	{
		return os << bs.to_string();
	}

}


#undef PLF_CPP20_SUPPORT
#undef PLF_CONSTFUNC
#undef PLF_CONSTEXPR
#undef PLF_NOEXCEPT
#undef PLF_EXCEPTIONS_SUPPORT
#undef PLF_TYPE_BITWIDTH
#undef PLF_BITSET_SIZE_BYTES
#undef PLF_ARRAY_CAPACITY
#undef PLF_ARRAY_END
#undef PLF_ARRAY_CAPACITY_BITS
#undef PLF_ARRAY_CAPACITY_BYTES

#endif // PLF_BITSET_H
