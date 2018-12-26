#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>
#include <getopt.h>
#include <immintrin.h>

// Virtual page size of the current system
const static std::size_t ByteBuffSize = sysconf(_SC_PAGE_SIZE);
const static std::size_t AsciiBuffSize = ByteBuffSize * 8;

struct Settings
{
	bool Decode        = false;
	bool IgnoreInvalid = false;
	std::size_t Wrap   = 76;
};

void Encode(
	std::uintmax_t InputFile,
	std::uintmax_t OutputFile,
	const Settings& EncodeSettings
)
{
	// Each byte of input will map to 8 bytes of output
	std::uint8_t* InputBuffer = static_cast<std::uint8_t*>(
		mmap(
			0,
			ByteBuffSize,
			PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS,
			-1,
			0
		)
	);
	std::uint64_t* OutputBuffer = static_cast<std::uint64_t*>(
		mmap(
			0,
			AsciiBuffSize,
			PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS,
			-1,
			0
		)
	);
	std::intmax_t CurRead = 0;
	while(
		(CurRead = read(InputFile, InputBuffer, ByteBuffSize)) > 0
	)
	{
		for( std::size_t i = 0; i < static_cast<std::size_t>(CurRead); ++i )
		{
		#if defined(__BMI2__)
			OutputBuffer[i] =
				__builtin_bswap64(
					_pdep_u64(
						static_cast<std::uint64_t>(InputBuffer[i]),
						0x0101010101010101
					) | 0x3030303030303030
				);
		#else
			OutputBuffer[i] =
				__builtin_bswap64(
					(((((
					static_cast<std::uint64_t>(InputBuffer[i])
					* 0x0101010101010101UL ) // "broadcast" low byte to all 8 bytes.
					& 0x8040201008040201UL ) // Mask each byte to have 1 unique bit.
					+ 0x00406070787C7E7FUL ) // Shift this bit to the last bit of each
											 // byte using the carry of binary addition.
					& 0x8080808080808080UL ) // Isolate these last bits of each byte.
					>> 7)					 // Shift it back to the low bit of each byte.
					| 0x3030303030303030UL   // Turn it into ascii '0' and '1'
			);
		#endif
		}
		write(OutputFile, OutputBuffer, CurRead * 8 );
	}
	munmap(InputBuffer, ByteBuffSize);
	munmap(OutputBuffer, AsciiBuffSize);
	if( CurRead < 0 )
	{
		std::puts(
			"Error reading input stream"
		);
	}
}

std::uint8_t DecodeWord( std::uint64_t BinAscii )
{
	const std::uint64_t& CurInput = __builtin_bswap64(BinAscii);
	std::uint8_t Binary = 0;
#if defined(__BMI2__)
	Binary = _pext_u64(CurInput,0x0101010101010101);
#else
	std::uint64_t Mask = 0x0101010101010101UL;
	for( std::uint64_t bb = 1UL; Mask != 0; bb += bb)
	{
		if( CurInput & Mask & -Mask )
		{
			Binary |= bb;
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
void Decode(
	std::uintmax_t InputFile,
	std::uintmax_t OutputFile,
	const Settings& DecodeSettings
)
{
	// Every 8 bytes of input will map to 1 byte of output
	std::uint64_t* InputBuffer = static_cast<std::uint64_t*>(
		mmap(
			0,
			AsciiBuffSize,
			PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS,
			-1,
			0
		)
	);
	std::uint8_t* OutputBuffer = static_cast<std::uint8_t*>(
		mmap(
			0,
			ByteBuffSize,
			PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS,
			-1,
			0
		)
	);

	std::size_t ToRead = AsciiBuffSize;
	// Number of bytes available for actual processing
	std::intmax_t CurRead = 0;
	// Process paged-sized batches of input in an attempt to have bulk-amounts of
	// conversions going on between calls to `read`
	while(
		(CurRead = read(
			InputFile,
			reinterpret_cast<std::uint8_t*>(InputBuffer) + (AsciiBuffSize - ToRead),
			ToRead
		))
	)
	{
		// Increase total bytes read

		// Validate input
		if( DecodeSettings.IgnoreInvalid )
		{
			for(
				std::size_t i = (AsciiBuffSize - ToRead);
				i < (AsciiBuffSize - ToRead + CurRead);
				++i
			)
			{
				// Find longest chain of garbage bytes from this position
				std::size_t GarbageLength = 0;
				while(
					(reinterpret_cast<std::uint8_t*>(InputBuffer)[i + GarbageLength] & 0xFE) != 0x30
				) GarbageLength++;
				// Move the rest of the array up, overlapping the garbage bytes
				if( GarbageLength < CurRead )
				{
					std::memmove(
						reinterpret_cast<std::uint8_t*>(InputBuffer) + (AsciiBuffSize - ToRead),
						reinterpret_cast<std::uint8_t*>(InputBuffer) + (AsciiBuffSize - ToRead) + GarbageLength,
						CurRead - GarbageLength
					);
				}
				CurRead -= GarbageLength;
			}
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
		write(OutputFile,OutputBuffer,CurRead / 8);

		// Set up for next read
		ToRead -= CurRead; 
		if( ToRead == 0 )
		{
			ToRead = AsciiBuffSize;
		}
	}
	munmap(InputBuffer, AsciiBuffSize);
	munmap(OutputBuffer, ByteBuffSize);
	if( CurRead < 0 )
	{
		std::puts(
			"Error reading input stream"
		);
	}
}
const static struct option CommandOptions[4] = {
	{ "decode",         optional_argument, nullptr,  'd' },
	{ "ignore-garbage", optional_argument, nullptr,  'i' },
	{ "wrap",           optional_argument, nullptr,  'w' },
	{ nullptr,                no_argument, nullptr, '\0' }
};

int main(int argc, char* argv[])
{
	Settings CurSettings = {};
	int Opt;
	int OptionIndex;
	while( (Opt = getopt_long(argc, argv, "diw:", CommandOptions, &OptionIndex )) != -1 )
	{
		switch( Opt )
		{
		case 'd': CurSettings.Decode = true;            break;
		case 'i': CurSettings.IgnoreInvalid = true;     break;
		case 'w': CurSettings.Wrap = std::atoi(optarg); break;
		default:
		{
			// TODO: Print usage
			return EXIT_FAILURE;
		}
		}
	}
	if( CurSettings.Decode )
	{
		Decode( STDIN_FILENO, STDOUT_FILENO, CurSettings );
	}
	else
	{
		Encode( STDIN_FILENO, STDOUT_FILENO, CurSettings );
	}
	return EXIT_SUCCESS;
}