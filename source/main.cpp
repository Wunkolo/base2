#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <sys/mman.h>
#include <unistd.h>
#include <immintrin.h>

// Virtual page size of the current system
const static std::size_t PageSize = sysconf(_SC_PAGE_SIZE);

void Encode(std::uintmax_t InputFile, std::uintmax_t OutputFile)
{
	std::uint8_t* InputBuffer = static_cast<std::uint8_t*>(
		mmap(
			0,
			PageSize,
			PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS,
			-1,
			0
		)
	);
	// Each byte of input will map to 8 bytes of output
	std::uint64_t* OutputBuffer = static_cast<std::uint64_t*>(
		mmap(
			0,
			PageSize * 8,
			PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS,
			-1,
			0
		)
	);
	std::size_t CurSize = 0;
	while(
		(CurSize = read(InputFile, InputBuffer, PageSize))
	)
	{
		for( std::size_t i = 0; i < CurSize; ++i )
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
		write(
			OutputFile,
			OutputBuffer,
			CurSize * 8
		);
	}
	munmap(InputBuffer ,PageSize    );
	munmap(OutputBuffer,PageSize * 8);
}

int main( int argc, char* argv[] )
{
	Encode( STDIN_FILENO, STDOUT_FILENO );
	return EXIT_SUCCESS;
}