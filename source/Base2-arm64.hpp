/// Encoding
#include <cstdint>
#include <cstddef>
#include <arm_neon.h>

namespace
{

// Recursive device
template<std::uint8_t WidthExp2>
inline void Encode(
	const std::uint8_t Input[], std::uint64_t Output[], std::size_t Length
)
{
	Encode<WidthExp2-1>(Input, Output, Length);
}

// Serial
template<>
inline void Encode<0>(
	const std::uint8_t Input[], std::uint64_t Output[], std::size_t Length
)
{
	// Least significant bit in an 8-bit integer
	const uint8x8_t MSB8 = vdup_n_u8(0b10000000);
	// Constant bits for ascii '0' and '1'
	const uint8x8_t BinAsciiBasis = vdup_n_u8('0');
	const uint64x1_t UniqueBit  = { 0x0102040810204080UL };
	const uint64x1_t CarryShift = { 0x7F7E7C7870604000UL };
	for( std::size_t i = 0; i < Length; ++i )
	{
		// Broadcast byte across 8 byte lanes
		uint8x8_t Word = vdup_n_u8(Input[i]);
		// Mask unique bits in each byte
		Word = vand_u8(Word, vreinterpret_u8_u64(UniqueBit));
		// Shift unique bit to upper bit of each 8-bit lane
		Word = vadd_u8(Word, vreinterpret_u8_u64(CarryShift));
		// Mask upper bit
		Word = vand_u8(Word, MSB8);
		// Shift it back down
		Word = vshr_n_u8(Word, 7u);
		// binary-or with '0'
		Word = vorr_u8(Word, BinAsciiBasis);
		// store
		vst1_u64(Output + i, vreinterpret_u64_u8(Word));
	}
}

// Two at a time
#if defined(__SSE2__)
template<>
inline void Encode<1>(
	const std::uint8_t Input[], std::uint64_t Output[], std::size_t Length
)
{
	constexpr std::uint64_t LSB8          = 0x0101010101010101UL;
	constexpr std::uint64_t UniqueBit     = 0x0102040810204080UL;
	constexpr std::uint64_t CarryShift    = 0x7F7E7C7870604000UL;

	std::size_t i = 0;
	for( ; i < Length; i += 2 )
	{
	#if defined(__SSSE3__)
		__m128i Result = _mm_set1_epi16(
			*reinterpret_cast<const std::uint16_t*>(&Input[i])
		);
		// Upper and lower 64-bits get filled with bytes
		Result = _mm_shuffle_epi8(Result, _mm_set_epi64x(LSB8 * 1, LSB8 * 0));
	#else
		__m128i Result = _mm_set_epi64x(
			LSB8 * static_cast<std::uint64_t>(Input[i + 1]),
			LSB8 * static_cast<std::uint64_t>(Input[i + 0])
		);
	#endif
		// Mask Unique bits per byte
		Result = _mm_and_si128(Result, _mm_set1_epi64x(UniqueBit));
		// Use the carry-bit to slide it to the far left
		Result = _mm_add_epi64(Result, _mm_set1_epi64x(CarryShift));
	#if defined(__SSE4_1__)
		// Pick between ascii '0' and '1', using the upper bit in each byte
		Result = _mm_blendv_epi8(
			_mm_set1_epi8('0'), _mm_set1_epi8('1'), Result
		);
	#else
		constexpr std::uint64_t BinAsciiBasis = LSB8 * '0';
		constexpr std::uint64_t MSB8          = LSB8 << 7u;
		// Mask this last bit
		Result = _mm_and_si128(Result, _mm_set1_epi64x(MSB8));
		// Shift it to the low bit of each byte
		Result = _mm_srli_epi64(Result, 7);
		// Convert it to ascii `0` and `1`
		Result = _mm_or_si128(Result, _mm_set1_epi64x(BinAsciiBasis));
	#endif
		_mm_store_si128(reinterpret_cast<__m128i*>(&Output[i]), Result);
	}

	Encode<0>(Input + i * 2, Output + i * 2, Length % 2);
}
#endif

}


void Base2::Encode(
	const std::uint8_t Input[], std::uint64_t Output[], std::size_t Length
)
{
	::Encode<0xFFu>(Input, Output, Length);
}


/// Decoding

namespace
{

// Recursive device
template<std::uint8_t WidthExp2>
inline void Decode(
	const std::uint64_t Input[], std::uint8_t Output[], std::size_t Length
)
{
	Decode<WidthExp2-1>(Input, Output, Length);
}


// Serial
template<>
inline void Decode<0>(
	const std::uint64_t Input[], std::uint8_t Output[], std::size_t Length
)
{
	for( std::size_t i = 0; i < Length; ++i )
	{
		std::uint8_t Binary = 0;
		const std::uint64_t ASCII = __builtin_bswap64(Input[i]);
		std::uint64_t Mask = 0x0101010101010101UL;
		for( std::uint64_t CurBit = 1UL; Mask != 0; CurBit <<= 1 )
		{
			if( ASCII & Mask & -Mask )
			{
				Binary |= CurBit;
			}
			Mask &= (Mask - 1UL);
		}
		Output[i] = Binary;
	}
}

// Two at a time
#if defined(__SSE2__)
template<>
inline void Decode<1>(
	const std::uint64_t Input[], std::uint8_t Output[], std::size_t Length
)
{
	constexpr std::uint64_t LSB8 = 0x0101010101010101UL;
	std::size_t i = 0;
	for( ; i < Length; i += 2 )
	{
		// Load in 16 bytes of endian-swapped ascii bytes
	#if defined(__SSSE3__)
		__m128i ASCII = _mm_loadu_si128(
			reinterpret_cast<const __m128i*>(&Input[i])
		);
		ASCII = _mm_shuffle_epi8(
			ASCII,
			_mm_set_epi64x(
				0x0001020304050607 + LSB8 * 0x08,
				0x0001020304050607 + LSB8 * 0x00
			)
		);
	#else
		__m128i ASCII = _mm_set_epi64x(
			__builtin_bswap64(Input[i + 1]), __builtin_bswap64(Input[i + 0])
		);
	#endif
		// Shift lowest bit of each byte into sign bit
		ASCII = _mm_slli_epi64(ASCII, 7);
		// Compress each sign bit into a 16-bit word
		*reinterpret_cast<std::uint16_t*>(&Output[i]) = _mm_movemask_epi8(ASCII);
	}

	Decode<0>(Input + i * 2, Output + i * 2, Length % 2);
}
#endif

}

void Base2::Decode(
	const std::uint64_t Input[], std::uint8_t Output[], std::size_t Length
)
{
	::Decode<0xFFu>(Input, Output, Length);
}

/// Filtering

std::size_t Base2::Filter(std::uint8_t Bytes[], std::size_t Length)
{
	std::size_t End = 0;
	std::size_t i = 0;
	// Check and compress 8 bytes at a time
	for( ; i + 7 < Length; i += 8 )
	{
		// Read in 8 bytes at once
		const std::uint64_t Word64 = *reinterpret_cast<const std::uint64_t*>(Bytes + i);

		// Check for valid bytes, in parallel
		if( (Word64 & 0xFEFEFEFEFEFEFEFE) == 0x3030303030303030 )
		{
			// We have 8 valid ascii-binary bytes
			*reinterpret_cast<std::uint64_t*>(Bytes + End) = Word64;
			End += 8;
		}
		else
		{
			// There is garbage
			for( std::size_t k = 0; k < 8; ++k )
			{
				const std::uint8_t CurByte = Bytes[i + k];
				if( (CurByte & 0xFE) != 0x30 ) continue;
				Bytes[End++] = CurByte;
			}
		}
	}

	for( ; i < Length; ++i )
	{
		const std::uint8_t CurByte = Bytes[i];
		if( (CurByte & 0xFE) != 0x30 ) continue;
		Bytes[End++] = CurByte;
	}
	return End;
}
