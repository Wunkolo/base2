#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <sys/mman.h>
#include <unistd.h>
#include <getopt.h>


// x86
#if defined(__x86_64__) || defined(_M_X64)
#include <x86intrin.h>
#else
// TODO: ARM
#endif

// Virtual page size of the current system
const static std::size_t ByteBuffSize = sysconf(_SC_PAGE_SIZE);
const static std::size_t AsciiBuffSize = ByteBuffSize * 8;

struct Settings
{
	std::FILE* InputFile  = stdin;
	std::FILE* OutputFile = stdout;
	bool Decode           = false;
	bool IgnoreInvalid    = false;
	std::size_t Wrap      = 76;
};

std::size_t WrapWrite(
	const char* Buffer,
	std::size_t Length,
	std::size_t WrapWidth,
	std::FILE* OutputFile,
	std::size_t CurrentColumn = 0
)
{
	if( WrapWidth == 0 )
	{
		return std::fwrite(Buffer, 1, Length, OutputFile);
	}
	for( std::size_t Written = 0; Written < Length; )
	{
		const std::size_t ColumnsRemaining = WrapWidth - CurrentColumn;
		const std::size_t ToWrite = std::min(
			ColumnsRemaining,
			Length - Written
		);
		if( ToWrite == 0)
		{
			std::fputc('\n',OutputFile);
			CurrentColumn = 0;
		}
		else
		{
			std::fwrite(Buffer + Written, 1, ToWrite, OutputFile);
			CurrentColumn += ToWrite;
			Written += ToWrite;
		}
	}
	return CurrentColumn;
}

bool Encode( const Settings& Settings )
{
	// Each byte of input will map to 8 bytes of output
	std::uint8_t* InputBuffer = static_cast<std::uint8_t*>(
		mmap(
			0, ByteBuffSize,
			PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS,
			-1, 0
		)
	);
	std::uint64_t* OutputBuffer = static_cast<std::uint64_t*>(
		mmap(
			0, AsciiBuffSize,
			PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS,
			-1, 0
		)
	);
	std::size_t CurrentColumn = 0;
	std::size_t CurRead = 0;
	while( (CurRead = std::fread(InputBuffer, 1, ByteBuffSize, Settings.InputFile)) )
	{
		// Process whatever was read
		std::size_t i = 0;
		// Four at a time
		for( std::size_t j = i / 4; j < CurRead / 4; ++j, i += 4 )
		{
			#if defined(__AVX2__)
			__m256i Result = _mm256_set1_epi32x(
				*reinterpret_cast<const std::uint32_t*>(&InputBuffer[i])
			);
			// Broadcast each byte to each 64-bit lane
			Result = _mm256_shuffle_epi8(
				Result,
				_mm256_set_epi64x(
					0x0101010101010101UL * 3,
					0x0101010101010101UL * 2,
					0x0101010101010101UL * 1,
					0x0101010101010101UL * 0
				)
			);
			// Mask Unique bits per byte
			Result = _mm256_and_si256(
				Result, _mm256_set1_epi64x(0x0102040810204080UL)
			);
			// Use the carry-bit to slide it to the far left
			Result = _mm256_add_epi64(
				Result, _mm256_set1_epi64x(0x7F7E7C7870604000UL)
			);
			// Mask this last bit
			Result = _mm256_and_si256(
				Result, _mm256_set1_epi64x(0x8080808080808080UL)
			);
			// Shift it to the low bit of each byte
			Result = _mm256_srli_epi64(Result, 7);
			// Convert it to ascii `0` and `1`
			Result = _mm256_or_si256(
				Result, _mm256_set1_epi64x(0x3030303030303030UL)
			);
			_mm256_storeu_si256(
				reinterpret_cast<__m256i*>(&OutputBuffer[i]), Result
			);
			#endif
		}
		// Two at a time
		for( std::size_t j = i / 2; j < CurRead / 2; ++j, i += 2 )
		{
			#if defined(__SSE2__)
			__m128i Result = _mm_set_epi64x(
				0x0101010101010101UL * static_cast<std::uint64_t>(InputBuffer[i + 1]),
				0x0101010101010101UL * static_cast<std::uint64_t>(InputBuffer[i + 0])
			);
			// Mask Unique bits per byte
			Result = _mm_and_si128(
				Result, _mm_set1_epi64x(0x0102040810204080UL)
			);
			// Use the carry-bit to slide it to the far left
			Result = _mm_add_epi64(
				Result, _mm_set1_epi64x(0x7F7E7C7870604000UL)
			);
			// Mask this last bit
			Result = _mm_and_si128(
				Result, _mm_set1_epi64x(0x8080808080808080UL)
			);
			// Shift it to the low bit of each byte
			Result = _mm_srli_epi64(Result, 7);
			// Convert it to ascii `0` and `1`
			Result = _mm_or_si128(
				Result, _mm_set1_epi64x(0x3030303030303030UL)
			);
			_mm_store_si128(
				reinterpret_cast<__m128i*>(&OutputBuffer[i]), Result
			);
			#endif
		}
		// One at a time
		for( ; i < CurRead; ++i )
		{
		#if defined(__BMI2__)
			OutputBuffer[i] = __builtin_bswap64(
				_pdep_u64(
					static_cast<std::uint64_t>(InputBuffer[i]),
					0x0101010101010101UL
				) | 0x3030303030303030UL
			);
		#else
			OutputBuffer[i] = (
				(((((
				static_cast<std::uint64_t>(InputBuffer[i])
				* 0x0101010101010101UL	) // "broadcast" low byte to all 8 bytes.
				& 0x0102040810204080UL	) // Mask each byte to have 1 unique bit.
				+ 0x7F7E7C7870604000UL	) // Shift this bit to the last bit of each
										  // byte using the carry of binary addition.
				& 0x8080808080808080UL	) // Isolate these last bits of each byte.
				>> 7					) // Shift it back to the low bit of each byte.
				| 0x3030303030303030UL	  // Turn it into ascii '0' and '1'
			);
		#endif
		}
		CurrentColumn = WrapWrite(
			reinterpret_cast<const char*>(OutputBuffer), CurRead * 8,
			Settings.Wrap, Settings.OutputFile, CurrentColumn
		);
	}
	if( std::ferror(Settings.InputFile) )
	{
		std::fputs("Error while reading input file",stderr);
	}
	munmap(InputBuffer,  ByteBuffSize);
	munmap(OutputBuffer, AsciiBuffSize);
	return EXIT_SUCCESS;
}

inline std::uint8_t DecodeWord( std::uint64_t BinAscii )
{
	const std::uint64_t& CurInput = __builtin_bswap64(BinAscii);
	std::uint8_t Binary = 0;
#if defined(__BMI2__)
	Binary = _pext_u64(CurInput,0x0101010101010101UL);
#else
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

// By default, Decode will extract the lowest set bit in a chunk of 8 bytes
// and compress it down into 1 byte.
// Even if the input is not '0'(0x30) or '1'(0x31) it will do this unless
// the settings explicitly say to ignore non-'0''1' garbage bytes.
bool Decode( const Settings& Settings )
{
	// Every 8 bytes of input will map to 1 byte of output
	std::uint64_t* InputBuffer = static_cast<std::uint64_t*>(
		mmap(
			0, AsciiBuffSize,
			PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS,
			-1, 0
		)
	);
	std::uint8_t* OutputBuffer = static_cast<std::uint8_t*>(
		mmap(
			0, ByteBuffSize,
			PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS,
			-1, 0
		)
	);

	std::size_t ToRead = AsciiBuffSize;
	// Number of bytes available for actual processing
	std::size_t CurRead = 0;
	// Process paged-sized batches of input in an attempt to have bulk-amounts of
	// conversions going on between calls to `read`
	while(
		(CurRead = std::fread(
			reinterpret_cast<std::uint8_t*>(InputBuffer) + (AsciiBuffSize - ToRead),
			1, ToRead, Settings.InputFile
		))
	)
	{
		// Filter input
		if( Settings.IgnoreInvalid )
		{
			const std::uint8_t* NewLast = std::remove_if(
				reinterpret_cast<std::uint8_t*>(InputBuffer) + (AsciiBuffSize - ToRead),
				reinterpret_cast<std::uint8_t*>(InputBuffer) + (AsciiBuffSize - ToRead + CurRead),
				[](const std::uint8_t& CurByte) { return (CurByte & 0xFE) != 0x30; }
			);
			const std::size_t RemovedBytes = 
				reinterpret_cast<std::uint8_t*>(InputBuffer) + (AsciiBuffSize - ToRead + CurRead) - NewLast;
			CurRead -= RemovedBytes;
		}
		// Process any new groups of 8 bytes that we can
		for(
			std::size_t i = (AsciiBuffSize - ToRead) / 8 ;
			i < (AsciiBuffSize - ToRead + CurRead) / 8 ;
			++i
		)
		{
			OutputBuffer[i] = DecodeWord(InputBuffer[i]);
		}
		if( std::fwrite(OutputBuffer, 1, CurRead / 8, Settings.OutputFile) != CurRead / 8 )
		{
			std::fputs("Error writing to output file", stderr);
			munmap(InputBuffer, AsciiBuffSize);
			munmap(OutputBuffer, ByteBuffSize);
			return EXIT_FAILURE;
		}

		// Set up for next read
		ToRead -= CurRead; 
		if( ToRead == 0 )
		{
			ToRead = AsciiBuffSize;
		}
	}
	munmap(InputBuffer, AsciiBuffSize);
	munmap(OutputBuffer, ByteBuffSize);
	if( std::ferror(Settings.InputFile) )
	{
		std::fputs("Error while reading input file",stderr);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

const char* Usage = 
"base2 - Wunkolo <wunkolo@gmail.com>\n"
"Usage: base2 [Options]... [File]\n"
"       base2 --decode [Options]... [File]\n"
"Options:\n"
"  -h, --help            Display this help/usage information\n"
"  -d, --decode          Decodes incoming binary ascii into bytes\n"
"  -i, --ignore-garbage  When decoding, ignores non-ascii-binary `0`, `1` bytes\n"
"  -w, --wrap=Columns    Wrap encoded binary output within columns\n"
"                        Default is `76`. `0` Disables linewrapping\n";

const static struct option CommandOptions[5] = {
	{ "decode",         optional_argument, nullptr,  'd' },
	{ "ignore-garbage", optional_argument, nullptr,  'i' },
	{ "wrap",           optional_argument, nullptr,  'w' },
	{ "help",           optional_argument, nullptr,  'h' },
	{ nullptr,                no_argument, nullptr, '\0' }
};

int main( int argc, char* argv[] )
{
	Settings CurSettings = {};
	int Opt;
	int OptionIndex;
	while( (Opt = getopt_long(argc, argv, "hdiw:", CommandOptions, &OptionIndex )) != -1 )
	{
		switch( Opt )
		{
		case 'd': CurSettings.Decode = true;            break;
		case 'i': CurSettings.IgnoreInvalid = true;     break;
		case 'w':
		{
			const std::intmax_t ArgWrap = std::atoi(optarg);
			if( ArgWrap < 0 )
			{
				std::fputs("Invalid wrap width", stderr);
				return EXIT_FAILURE;
			}
			CurSettings.Wrap = ArgWrap;
			break;
		}
		case 'h':
		{
			std::puts(Usage);
			return EXIT_SUCCESS;
		}
		default:
		{
			return EXIT_FAILURE;
		}
		}
	}
	if( optind < argc )
	{
		if( std::strcmp(argv[optind],"-") != 0 )
		{
			CurSettings.InputFile = fopen(argv[optind],"rb");
			if( CurSettings.InputFile == nullptr )
			{
				std::fprintf(
					stderr,
					"Error opening input file: %s\n",
					argv[optind]
				);
			}
		}
	}
	return (CurSettings.Decode ? Decode:Encode)(CurSettings);
}
