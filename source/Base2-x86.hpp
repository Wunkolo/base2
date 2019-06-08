#include <x86intrin.h>


namespace
{

// Recursive device
template<std::uint8_t WidthExp2>
inline void Encode(
	const std::uint8_t* Input, std::uint64_t* Output, std::size_t Length
)
{
	Encode<WidthExp2-1>(Input, Output, Length);
}

// Serial
template<>
inline void Encode<0>(
	const std::uint8_t* Input, std::uint64_t* Output, std::size_t Length
)
{
	// Least significant bit in an 8-bit integer
	constexpr std::uint64_t LSB8       = 0x0101010101010101UL;
	// Constant bits for ascii '0' and '1'
	constexpr std::uint64_t BinAsciiBasis = LSB8 * '0';
#if defined (__BMI2__)
	for( std::size_t i = 0; i < Length; ++i )
	{
		Output[i] = __builtin_bswap64(
			_pdep_u64(
				static_cast<std::uint64_t>(Input[i]), LSB8
			) | BinAsciiBasis
		);
	}
#else
	for( std::size_t i = 0; i < Length; ++i )
	{
		constexpr std::uint64_t UniqueBit  = 0x0102040810204080UL;
		constexpr std::uint64_t CarryShift = 0x7F7E7C7870604000UL;
		constexpr std::uint64_t MSB8       = LSB8 << 7u;
		Output[i] = ((((((
			static_cast<std::uint64_t>(Input[i])
			* LSB8			) & UniqueBit		)
			+ CarryShift	) & MSB8			)
			>> 7			) | BinAsciiBasis	);
	}
#endif
}

#if defined(__SSE2__)
// Two at a time
template<>
inline void Encode<1>(
	const std::uint8_t* Input, std::uint64_t* Output, std::size_t Length
)
{
	constexpr std::uint64_t LSB8          = 0x0101010101010101UL;
	constexpr std::uint64_t UniqueBit     = 0x0102040810204080UL;
	constexpr std::uint64_t CarryShift    = 0x7F7E7C7870604000UL;
	constexpr std::uint64_t MSB8          = LSB8 << 7u;
	constexpr std::uint64_t BinAsciiBasis = LSB8 * '0';

	std::size_t i = 0;
	for( ; i < Length; i += 2 )
	{
		__m128i Result = _mm_set_epi64x(
			LSB8 * static_cast<std::uint64_t>(Input[i + 1]),
			LSB8 * static_cast<std::uint64_t>(Input[i + 0])
		);
		// Mask Unique bits per byte
		Result = _mm_and_si128(Result, _mm_set1_epi64x(UniqueBit));
		// Use the carry-bit to slide it to the far left
		Result = _mm_add_epi64(Result, _mm_set1_epi64x(CarryShift));
		// Mask this last bit
		Result = _mm_and_si128(Result, _mm_set1_epi64x(MSB8));
		// Shift it to the low bit of each byte
		Result = _mm_srli_epi64(Result, 7);
		// Convert it to ascii `0` and `1`
		Result = _mm_or_si128(Result, _mm_set1_epi64x(BinAsciiBasis));
		_mm_store_si128(reinterpret_cast<__m128i*>(&Output[i]), Result);
	}

	Encode<0>(Input + i * 2, Output + i * 2, Length % 2);
}
#endif

#if defined(__AVX2__)
// Four at a time
template<>
inline void Encode<2>(
	const std::uint8_t* Input, std::uint64_t* Output, std::size_t Length
)
{
	constexpr std::uint64_t LSB8          = 0x0101010101010101UL;
	constexpr std::uint64_t UniqueBit     = 0x0102040810204080UL;
	constexpr std::uint64_t CarryShift    = 0x7F7E7C7870604000UL;
	constexpr std::uint64_t MSB8          = LSB8 << 7u;
	constexpr std::uint64_t BinAsciiBasis = LSB8 * '0';

	std::size_t i = 0;
	for( ; i < Length; i += 4 )
	{
		__m256i Result = _mm256_set1_epi32(
			*reinterpret_cast<const std::uint32_t*>(&Input[i])
		);
		// Broadcast each byte to each 64-bit lane
		Result = _mm256_shuffle_epi8(
			Result, _mm256_set_epi64x(LSB8 * 3, LSB8 * 2, LSB8 * 1, LSB8 * 0)
		);
		// Mask Unique bits per byte
		Result = _mm256_and_si256(Result, _mm256_set1_epi64x(UniqueBit));
		// Use the carry-bit to slide it to the far left
		Result = _mm256_add_epi64(Result, _mm256_set1_epi64x(CarryShift));
		// Mask this last bit
		Result = _mm256_and_si256(Result, _mm256_set1_epi64x(MSB8));
		// Shift it to the low bit of each byte
		Result = _mm256_srli_epi64(Result, 7);
		// Convert it to ascii `0` and `1`
		Result = _mm256_or_si256(Result, _mm256_set1_epi64x(BinAsciiBasis));
		_mm256_storeu_si256( reinterpret_cast<__m256i*>(&Output[i]), Result);
	}

	Encode<1>(Input + i * 4, Output + i * 4, Length % 4);
}
#endif

#if defined(__AVX512F__) && defined(__AVX512BW__)
// Eight at a time
template<>
inline void Encode<3>(
	const std::uint8_t* Input, std::uint64_t* Output, std::size_t Length
)
{
	constexpr std::uint64_t LSB8          = 0x0101010101010101UL;
	constexpr std::uint64_t UniqueBit     = 0x0102040810204080UL;

	std::size_t i = 0;
	for( ; i < Length; i += 8 )
	{
		// Load 8 bytes, and broadcast it across all 8 64-bit lanes
		__m512i Bytes8 = _mm512_set1_epi64(
			*reinterpret_cast<const std::uint64_t*>(&Input[i])
		);
		// "Unzip" each byte across each 64-bit lane
		Bytes8 = _mm512_shuffle_epi8(
			Bytes8, _mm512_set_epi64(
				LSB8 * 7, LSB8 * 6, LSB8 * 5, LSB8 * 4,
				LSB8 * 3, LSB8 * 2, LSB8 * 1, LSB8 * 0
			)
		);
		// Get unique bits in each byte into a 64-bit mask
		const __mmask64 BitMask = _mm512_test_epi8_mask(
			Bytes8, _mm512_set1_epi64(UniqueBit)
		);
		// Use the mask to select between ASCII bytes `0` and `1`
		const __m512i ASCII = _mm512_mask_blend_epi8(
			BitMask, _mm512_set1_epi8('0'), _mm512_set1_epi8('1')
		);
		_mm512_storeu_si512( reinterpret_cast<__m512i*>(&Output[i]), ASCII);
	}

	Encode<2>(Input + i * 8, Output + i * 8, Length % 8);
}
#endif
}


void Base2::Encode(
	const std::uint8_t* Input, std::uint64_t* Output, std::size_t Length
)
{
	::Encode<~0>(Input, Output, Length);
}
