#include <Base2.hpp>

#if defined(__x86_64__) || defined(_M_X64)
#include "Base2-x86.hpp"
#else
// Generic Implementation
void Base2::Encode(
	const std::uint8_t* Input, std::uint64_t* Output, std::size_t Length
)
{
	// Least significant bit in an 8-bit integer
	constexpr std::uint64_t LSB8       = 0x0101010101010101UL;
	// Each byte has a unique bit set
	constexpr std::uint64_t UniqueBit  = 0x0102040810204080UL;
	// Shifts unique bits to the left, using the carry of binary addition
	constexpr std::uint64_t CarryShift = 0x7F7E7C7870604000UL;
	// Most significant bit in an 8-bit integer
	constexpr std::uint64_t MSB8       = LSB8 << 7u;
	// Constant bits for ascii '0' and '1'
	constexpr std::uint64_t BinAsciiBasis = LSB8 * '0';
	for( std::size_t i = 0; i < Length; ++i )
	{
		Output[i] = ((((((
			static_cast<std::uint64_t>(Input[i])
			* LSB8			) // "broadcast" low byte to all 8 bytes.
			& UniqueBit		) // Mask each byte to have 1 unique bit.
			+ CarryShift	) // Shift this bit to the last bit of each
							  // byte using the carry of binary addition.
			& MSB8			) // Isolate these last bits of each byte.
			>> 7			) // Shift it back to the low bit of each byte.
			| BinAsciiBasis	  // Turn it into ascii '0' and '1'
		);
	}
}
#endif
