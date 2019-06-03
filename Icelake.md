This is a little writeup on some anticipatory code to eventually test and benchmark on the upcoming Intel Icelake architecture.

# Decoding

The `pext` instruction is a particularly useful instruction in [BMI2](https://en.wikipedia.org/wiki/Bit_Manipulation_Instruction_Sets) that allows the programmer to provide
a bit-mask integer with `1` bits set in positions of interests for which the
`pext` instruction will **ext**ract these bits in **p**arallel and compact them all against the least-significnat bits.
```
Given a bitmask and an input, pext will select the bits where-ever there is a
set bit in the mask, and compress them together to produce a new result.

|0000100000001111100000100001111100010000000010000001001000010000|  < Operand A
  ^      ^             ^       ^                ^      ^    ^   ^
|0100000010000000000000100000001000000000000000010000001000010001|  < Mask
                                   | Extract bits at mask
                                   V
|.1......0.............1.......1................0......1....1...0|
                                   | Compress into new 64-bit integer
                                   V
|0000000000000000000000000000000000000000000000000000000000110010|  < Result
```

This instruction is very useful for tasks such as converting ascii [base2](http://0x80.pl/notesen/2014-10-06-pext-convert-ascii-bin-to-num.html)
and [hexadecimal](http://0x80.pl/notesen/2014-10-09-pext-convert-ascii-hex-to-num.html) back into its original bytes of data and other uses in [text-processing](http://0x80.pl/notesen/2019-01-05-avx512vbmi-remove-spaces.html#pext-based-algorithm-new).

`pext` only has a 32-bit and 64-bit variants to process 4 or 8 bytes of data
at once within a general-purpose register so it is only capable of processing
one base-2 byte of data(8 ascii-characters of `0` and `1`) back into a regular byte
at a time(it also requires a `bswap64` due to endian-issues):

```cpp
// Goes from ascii "00100101" to binary byte 0b00100101('a')
// the ascii string "00100101" is 8 bytes, so it fits perfectly
// with a 64-bit integer
inline std::uint8_t DecodeBase2Word( std::uint64_t BinAscii )
{
	const std::uint64_t CurInput = __builtin_bswap64(BinAscii);
	std::uint8_t Binary = 0;
#if defined(__BMI2__)
	// Much faster, or is it?
	Binary = _pext_u64(CurInput, 0x0101010101010101UL);
#else
	// Serial bit extraction
	std::uint64_t Mask = 0x0101010101010101UL;
	for( std::uint64_t CurBit = 1UL; Mask != 0; CurBit <<= 1 )
	{
		if( CurInput & Mask & -Mask )
		{
			Binary |= CurBit;
		}
		Mask &= (Mask - 1UL);
	}
#endif
	return Binary;
}
```

---

[Icelake](https://en.wikipedia.org/wiki/AVX-512#CPUs_with_AVX-512) introduces the `AVX512` variant [BITALG](https://en.wikipedia.org/wiki/AVX-512#New_instructions_in_AVX-512_VPOPCNTDQ_and_AVX-512_BITALG) which provides the intrinsic
`_mm_bitshuffle_epi64_mask` which emits [vpshufbitqmb](https://github.com/HJLebbink/asm-dude/wiki/VPSHUFBITQMB).

This instruction will go through each 64-bit lane in the second operand and
treat it as eight 8-bit index values(modulo 64) into the bits of the other operand's
64-bit lane, then it will compact these 8 selected bits into a new 8-bit
integer within the AVX512 mask-register.

While this doesn't use a bitmask like `pext`, it allow 64-bit integers
to be treated as what is basically a Look Up Table of 64-bits to produce a new
8-bit byte. Not only that, but it operates upon 64-bit lanes within SIMD registers allowing
for up to 8 bytes to be converted at a time in parallel.
2 bytes can be converted at a time in a 128-bit SSE register, 4 at a time in a
256-bit AVX register, and 8 at a time in a 512-bit register.


```
  3210987654321098765432109876543210987654321098765432109876543210
  666655555555554444444444333333333322222222221111111111
  -----------------------------------------------------------------
  0000100000001111100000100001111100010000000010000001001000010000| < Operand A
  |^      ^             ^       ^     +----------^      ^    ^   ^|
  ||      |             |       |     |       +---------+    |+--+|
  |+--+   +---+       +-+     +-+     |       |       +------+|   |
  |   |       |       |       |       |       |       |       |   |
  |---+-------+-------+-------+-------+-------+-------+-------+---|
  |  62   |  55   |  41   |  33   |  16   |   9   |   4   |   0   | < Operand B
  +---------------------------------------------------------------+
                                  | Get bits at index
                                  V
  +---------------------------------------------------------------+
  |   0   |   0   |   1   |   1   |   0   |   0   |   1   |   0   |
  +---------------------------------------------------------------+
                                  | Compress into new 8-bit integer
                                  V
                          +----------------+
                          |   0b00110010   |
                          +----------------+
```

With this, a basic greedy algorithm can be made to process different widths of base2 ascii
back into its original bytes. The least significant bit of the ascii bytes for `0` and `1` are also 
`0` and `1`. By extracting and compacting these bits together, we can convert
ascii bytes back into their original bytes.

```cpp

// '0' : 0b00110000
// '1' : 0b00110001
//                ^ Extract and compress these bits
//                  the rest of he bits stay the same! (0x30)
//                  (assuming you've validated your input)

void Base2Decode(
    const std::uint64_t Input[], std::uint8_t Output[], std::size_t Length
)
{
	std::size_t i = 0;
	// 8 at a time
	for( std::size_t j = i/8 ; i < Length/8; ++j, i += 8 )
	{
		const __mmask64 Compressed = _mm512_bitshuffle_epi64_mask(
			_mm512_loadu_si512(reinterpret_cast<const __m512i*>(Input + i)),
			_mm512_set1_epi64(0x00'08'10'18'20'28'30'38)
		);
		_store_mask64(reinterpret_cast<__mmask64*>(Output + i), Compressed);
	}
	// 4 at a time
	for( std::size_t j = i/4 ; i < Length/4; ++j, i += 4 )
	{
		const __mmask32 Compressed = _mm256_bitshuffle_epi64_mask(
			_mm256_loadu_si256(reinterpret_cast<const __m256i*>(Input + i)),
			_mm256_set1_epi64x(0x00'08'10'18'20'28'30'38)
		);
		_store_mask32(reinterpret_cast<__mmask32*>(Output + i), Compressed);
	}
	// 2 at a time
	for( std::size_t j = i/2 ; i < Length/2; ++j, i += 2 )
	{
		const __mmask16 Compressed = _mm_bitshuffle_epi64_mask(
			_mm_loadu_si128(reinterpret_cast<const __m128i*>(Input + i)),
			_mm_set1_epi64x(0x00'08'10'18'20'28'30'38)
		);
		_store_mask16(reinterpret_cast<__mmask16*>(Output + i), Compressed);
	}
	// Serial(could probably just use the pext implementation here but I'm demonstrating bitshuffle_epi64 here)
	for( ; i < Length; ++i )
	{
		const __mmask16 Compressed = _mm_bitshuffle_epi64_mask(
			_mm_loadl_epi64(reinterpret_cast<const __m128i*>(Input + i)),
			_mm_set1_epi64x(0x00'08'10'18'20'28'30'38)
		);
		Output[i] = static_cast<std::uint8_t>(_cvtmask16_u32(Compressed));
	}
}

int main()
{
	// "Hello World!"
	const std::uint64_t* Input
	= (const std::uint64_t*)"010010000110010101101100011011000110111100100000010101110110111101110010011011000110010000100001";
	std::uint8_t Output[12] = {0};

	Base2Decode(Input, Output, 12);
	std::printf("Output: '%.12s'\n", Output);
}
```

As of now(Thu 30 May 2019 01:53:47 PM PDT) Icelake is not out yet so there is no
way for me to actually benchmark this on hardware to get some performance numbers
but it is theretically up to **x8** times faster than the _already-fast_ `pext` method
of decoding base2-ascii back into binary!

# Encoding

Similar to decoding, the `vpshufbitqmb` instruction can be used to distribute bits across bytes to accelerate
base2 _encoding_. `vpshufbitqmb` already creates a 64-bit mask which lends itself to the the masked AVX512
operations. The `_mm512_mask_blend_epi8` intrinsic can utilize this 64-bit mask to "pick" between the `0` and `1`
ascii bytes. Across a 512-bit register(64 bytes), **8** bytes of input data can be processed at a time providing again
another theoretical **x8** speedup.

```cpp

void Base2Encode(
    const std::uint8_t Input[], std::uint64_t Output[], std::size_t Length
)
{
	std::size_t i = 0;
	// Encode 8 bytes at a time!
    for( std::size_t j = i/8 ; i < Length/8; ++j, i += 8 )
    {
		// Reverse bits in each byte and convert it into an AVX512 mask,
		// all in one instruction.
		const __mmask64 Mask = _mm512_bitshuffle_epi64_mask(
			_mm512_set1_epi64(*(const std::uint64_t*)&Input[i]),
			_mm512_set_epi64(
				0x00'01'02'03'04'05'06'07 + 0x0101010101010101 * 0x38, // Byte 7
				0x00'01'02'03'04'05'06'07 + 0x0101010101010101 * 0x30, // Byte 6
				0x00'01'02'03'04'05'06'07 + 0x0101010101010101 * 0x28, // Byte 5
				0x00'01'02'03'04'05'06'07 + 0x0101010101010101 * 0x20, // Byte 4
				0x00'01'02'03'04'05'06'07 + 0x0101010101010101 * 0x18, // Byte 3
				0x00'01'02'03'04'05'06'07 + 0x0101010101010101 * 0x10, // Byte 2
				0x00'01'02'03'04'05'06'07 + 0x0101010101010101 * 0x08, // Byte 1
				0x00'01'02'03'04'05'06'07 + 0x0101010101010101 * 0x00  // Byte 0
			)
		);
		// Use 64-bit mask to create 64 ascii-bytes(8 encoded bytes)
        // by picking between '0' and '1' bytes
		const __m512i Ascii = _mm512_mask_blend_epi8(
			Mask, _mm512_set1_epi8('0'), _mm512_set1_epi8('1')
		);
		_mm512_storeu_si512(&Output[i], Ascii);
	}
	// Other versions are just variations of the above at different widths
	// for( std::size_t j = i/4 ; i < Length/4; ++j, i += 4 ) ...
	// for( std::size_t j = i/2 ; i < Length/2; ++j, i += 2 ) ...
	for( ; i < Length; ++i )
	{
        // Currently the fastest method
		Output[i] = __builtin_bswap64(
			_pdep_u64( static_cast<std::uint64_t>(Input[i]), 0x0101010101010101)
			| (0x0101010101010101 * '0')
		);
	}
}

int main()
{
	const char* Input = "Hello World!";
	char Output[1024] = {0};

	Base2Encode((std::uint8_t*)Input, (std::uint64_t*)Output, 12);
	std::printf("Output: '%.1024s'\n", Output);
	// 010010000110010101101100011011000110111100100000010101110110111101110010011011000110010000100001
}
```
