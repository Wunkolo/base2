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
	// Constant bits for ascii '0' and '1'
	const uint8x8_t BinAsciiBasis = vdup_n_u8('0');
	const int8x8_t UniqueBit  = { 0, 1, 2, 3, 4, 5, 6, 7 };
	for( std::size_t i = 0; i < Length; ++i )
	{
		// Broadcast byte across 8 byte lanes
		uint8x8_t Word = vld1_dup_u8(Input + i);
		// Shift Unique bits into the upper bit of each byte
		Word = vshl_u8(Word, UniqueBit);
		// Shift and "or" it using binary addition
		Word = vsra_n_u8(BinAsciiBasis, Word, 7);
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
	// Constant bits for ascii '0' and '1'
	const uint8x16_t BinAsciiBasis = vdupq_n_u8('0');
	const int8x16_t UniqueBit  = {
		0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7
	};
	std::size_t i = 0;
	for( ; i < Length; i += 2 )
	{
		const uint8x8x2_t Input2 = vld2_dup_u8(Input + i);
		// Broadcast byte across 8 byte lanes
		uint8x16_t Word2 = vcombine_u8(Input2.val[0], Input2.val[1]);
		// Shift Unique bits into the upper bit of each byte
		Word2 = vshlq_u8(Word2, UniqueBit);
		// Shift and "or" it using binary addition
		Word2 = vsraq_n_u8(BinAsciiBasis, Word2, 7);
		// Store
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
	// Constant bits for ascii '0' and '1'
	const uint8x16_t BinAsciiBasis = vdupq_n_u8('0');
	const int8x16_t UniqueBit  = {
		0, 1, 2, 3, 4, 5, 6, 7,
		0, 1, 2, 3, 4, 5, 6, 7
	};
	std::size_t i = 0;
	for( ; i < Length; i += 4 )
	{
		const uint8x8x4_t Input4 = vld4_dup_u8(Input + i);
		// Broadcast byte across 8 byte lanes
		uint8x16x2_t Word4 = {
			vcombine_u8(Input4.val[0], Input4.val[1]),
			vcombine_u8(Input4.val[2], Input4.val[3])
		};
		// Shift Unique bits into the upper bit of each byte
		Word4.val[0] = vshlq_u8(Word4.val[0], UniqueBit);
		Word4.val[1] = vshlq_u8(Word4.val[1], UniqueBit);
		// Shift and "or" it using binary addition
		Word4.val[0] = vsraq_n_u8(BinAsciiBasis, Word4.val[0], 7);
		Word4.val[1] = vsraq_n_u8(BinAsciiBasis, Word4.val[1], 7);
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
	const int8x8_t Shift = {
		-7, -6, -5, -4, -3, -2, -1, 0
	};
	for( std::size_t i = 0; i < Length; ++i )
	{
		uint8x8_t ASCII = vld1_u8(
			reinterpret_cast<const std::uint8_t*>(Input + i)
		);
		// Endian swap
		ASCII = vrev64_u8(ASCII);
		// Push each of the low bits to the high bit
		ASCII = vshl_n_u8(ASCII, 7);
		// Shift each bit into a unique position
		ASCII = vshl_u8(ASCII, Shift);
		// Horizontally reduce bytes, using "add" as "or" since each bit is
		// uniquely positioned
		Output[i] = vaddv_u8(ASCII);
	}
}

// Two at a time
template<>
inline void Decode<1>(
	const std::uint64_t Input[], std::uint8_t Output[], std::size_t Length
)
{
	const int8x16_t Shift = {
		-7, -6, -5, -4, -3, -2, -1, 0,
		-7, -6, -5, -4, -3, -2, -1, 0
	};
	std::size_t i = 0;
	for(; i < Length; i += 2 )
	{
		uint8x16_t ASCII = vld1q_u8(
			reinterpret_cast<const std::uint8_t*>(Input + i)
		);
		// Endian swap
		ASCII = vrev64q_u8(ASCII);
		// Push each of the low bits to the high bit
		ASCII = vshlq_n_u8(ASCII, 7);
		// Shift each bit into a unique position
		ASCII = vshlq_u8(ASCII, Shift);
		// Horizontally reduce bytes, using "add" as "or" since each bit is
		// uniquely positioned
		Output[i + 0] = vaddv_u8(vget_low_u8(ASCII));
		Output[i + 1] = vaddv_u8(vget_high_u8(ASCII));
	}

	Decode<0>(Input + i * 2, Output + i * 2, Length % 2);
}

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
