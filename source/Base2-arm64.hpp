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
		uint8x8_t Word = vld1_dup_u8(Input + i);
		// Mask unique bits in each byte
		Word = vand_u8(Word, vreinterpret_u8_u64(UniqueBit));
		// Shift unique bit to upper bit of each 8-bit lane
		Word = vadd_u8(Word, vreinterpret_u8_u64(CarryShift));
		// Mask upper bit
		Word = vand_u8(Word, MSB8);
		// Shift it back down
		Word = vshr_n_u8(Word, 7u);
		// Binary-or with '0'
		Word = vorr_u8(Word, BinAsciiBasis);
		// Store
		vst1_u64(Output + i, vreinterpret_u64_u8(Word));
	}
}

// Two at a time
template<>
inline void Encode<1>(
	const std::uint8_t Input[], std::uint64_t Output[], std::size_t Length
)
{
	// Least significant bit in an 8-bit integer
	const uint8x16_t MSB8 = vdupq_n_u8(0b10000000);
	// Constant bits for ascii '0' and '1'
	const uint8x16_t BinAsciiBasis = vdupq_n_u8('0');
	const uint64x2_t UniqueBit  = vdupq_n_u64(0x0102040810204080UL);
	const uint64x2_t CarryShift = vdupq_n_u64(0x7F7E7C7870604000UL);
	std::size_t i = 0;
	for( ; i < Length; i += 2 )
	{
		const uint8x8x2_t Input2 = vld2_dup_u8(Input + i);
		// Broadcast byte across 8 byte lanes
		uint8x16_t Word2 = vcombine_u8(Input2.val[0], Input2.val[1]);
		// Mask unique bits in each byte
		Word2 = vandq_u8(Word2, vreinterpretq_u8_u64(UniqueBit));
		// Shift unique bit to upper bit of each 8-bit lane
		Word2 = vaddq_u8(Word2, vreinterpretq_u8_u64(CarryShift));
		// Mask upper bit
		Word2 = vandq_u8(Word2, MSB8);
		// Shift it back down
		Word2 = vshrq_n_u8(Word2, 7u);
		// binary-or with '0'
		Word2 = vorrq_u8(Word2, BinAsciiBasis);
		// store
		vst1q_u64(Output + i, vreinterpretq_u64_u8(Word2));
	}

	Encode<0>(Input + i * 2, Output + i * 2, Length % 2);
}

// Four at a time
template<>
inline void Encode<2>(
	const std::uint8_t Input[], std::uint64_t Output[], std::size_t Length
)
{
	// Least significant bit in an 8-bit integer
	const uint8x16_t MSB8 = vdupq_n_u8(0b10000000);
	// Constant bits for ascii '0' and '1'
	const uint8x16_t BinAsciiBasis = vdupq_n_u8('0');
	const uint64x2_t UniqueBit  = vdupq_n_u64(0x0102040810204080UL);
	const uint64x2_t CarryShift = vdupq_n_u64(0x7F7E7C7870604000UL);
	std::size_t i = 0;
	for( ; i < Length; i += 4 )
	{
		const uint8x8x4_t Input4 = vld4_dup_u8(Input + i);
		// Broadcast byte across 8 byte lanes
		uint8x16x2_t Word4 = {
			vcombine_u8(Input4.val[0], Input4.val[1]),
			vcombine_u8(Input4.val[2], Input4.val[3])
		};
		// Mask unique bits in each byte
		Word4.val[0] = vandq_u8(Word4.val[0], vreinterpretq_u8_u64(UniqueBit));
		Word4.val[1] = vandq_u8(Word4.val[1], vreinterpretq_u8_u64(UniqueBit));
		// Shift unique bit to upper bit of each 8-bit lane
		Word4.val[0] = vaddq_u8(Word4.val[0], vreinterpretq_u8_u64(CarryShift));
		Word4.val[1] = vaddq_u8(Word4.val[1], vreinterpretq_u8_u64(CarryShift));
		// Shift unique bit to upper bit of each 8-bit lane
		// Mask upper bit
		Word4.val[0] = vandq_u8(Word4.val[0], MSB8);
		Word4.val[1] = vandq_u8(Word4.val[1], MSB8);
		// Shift it back down
		Word4.val[0] = vshrq_n_u8(Word4.val[0], 7u);
		Word4.val[1] = vshrq_n_u8(Word4.val[1], 7u);
		// Binary-or with '0'
		Word4.val[0] = vorrq_u8(Word4.val[0], BinAsciiBasis);
		Word4.val[1] = vorrq_u8(Word4.val[1], BinAsciiBasis);
		// Store
		//vst1q_u8_x2((uint8_t*)(Output + i), Word4);
		vst1q_u64(Output + i + 0, vreinterpretq_u64_u8(Word4.val[0]));
		vst1q_u64(Output + i + 2, vreinterpretq_u64_u8(Word4.val[1]));
	}

	Encode<1>(Input + i * 4, Output + i * 4, Length % 4);
}

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
