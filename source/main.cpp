#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <getopt.h>
#include <sys/mman.h>
#include <unistd.h>

#include <Base2.hpp>

// Virtual page size of the current system
const static std::size_t ByteBuffSize  = sysconf(_SC_PAGE_SIZE);
const static std::size_t AsciiBuffSize = ByteBuffSize * 8;

struct Settings
{
	std::FILE*  InputFile     = stdin;
	std::FILE*  OutputFile    = stdout;
	bool        Decode        = false;
	bool        IgnoreInvalid = false;
	std::size_t Wrap          = 76;
};

std::size_t WrapWrite(
	const char* Buffer, std::size_t Length, std::size_t WrapWidth,
	std::FILE* OutputFile, std::size_t CurrentColumn = 0
)
{
	if( WrapWidth == 0 )
	{
		return std::fwrite(Buffer, 1, Length, OutputFile);
	}
	for( std::size_t Written = 0; Written < Length; )
	{
		const std::size_t ColumnsRemaining = WrapWidth - CurrentColumn;
		const std::size_t ToWrite
			= std::min(ColumnsRemaining, Length - Written);
		if( ToWrite == 0 )
		{
			std::fputc('\n', OutputFile);
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

bool Encode(const Settings& Settings)
{
	// Each byte of input will map to 8 bytes of output
	std::uint8_t*  InputBuffer   = static_cast<std::uint8_t*>(mmap(
        0, ByteBuffSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,
        -1, 0
    ));
	std::uint64_t* OutputBuffer  = static_cast<std::uint64_t*>(mmap(
        0, AsciiBuffSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,
        -1, 0
    ));
	std::size_t    CurrentColumn = 0;
	std::size_t    CurRead       = 0;
	while(
		(CurRead = std::fread(InputBuffer, 1, ByteBuffSize, Settings.InputFile))
	)
	{
		Base2::Encode(InputBuffer, OutputBuffer, CurRead);
		CurrentColumn = WrapWrite(
			reinterpret_cast<const char*>(OutputBuffer), CurRead * 8,
			Settings.Wrap, Settings.OutputFile, CurrentColumn
		);
	}
	if( std::ferror(Settings.InputFile) )
	{
		std::fputs("Error while reading input file", stderr);
	}
	munmap(InputBuffer, ByteBuffSize);
	munmap(OutputBuffer, AsciiBuffSize);
	return EXIT_SUCCESS;
}

// By default, Decode will extract the lowest set bit in a chunk of 8 bytes
// and compress it down into 1 byte.
// Even if the input is not '0'(0x30) or '1'(0x31) it will do this unless
// the settings explicitly say to ignore non-'0''1' garbage bytes.
bool Decode(const Settings& Settings)
{
	// Every 8 bytes of input will map to 1 byte of output
	std::uint64_t* InputBuffer  = static_cast<std::uint64_t*>(mmap(
        0, AsciiBuffSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,
        -1, 0
    ));
	std::uint8_t*  OutputBuffer = static_cast<std::uint8_t*>(mmap(
        0, ByteBuffSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,
        -1, 0
    ));

	std::size_t ToRead = AsciiBuffSize;
	// Number of bytes available for actual processing
	std::size_t CurRead = 0;
	// Process paged-sized batches of input in an attempt to have bulk-amounts
	// of conversions going on between calls to `read`
	while(
		(CurRead = std::fread(
			 reinterpret_cast<std::uint8_t*>(InputBuffer)
				 + (AsciiBuffSize - ToRead),
			 1, ToRead, Settings.InputFile
		 ))
	)
	{
		// Filter input of all garbage bytes
		if( Settings.IgnoreInvalid )
		{
			CurRead = Base2::Filter(
				reinterpret_cast<std::uint8_t*>(InputBuffer)
					+ (AsciiBuffSize - ToRead),
				CurRead
			);
		}
		// Process any new groups of 8 ascii-bytes
		Base2::Decode(
			InputBuffer + (AsciiBuffSize - ToRead) / 8, OutputBuffer,
			CurRead / 8
		);
		if( std::fwrite(OutputBuffer, 1, CurRead / 8, Settings.OutputFile)
			!= CurRead / 8 )
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
		std::fputs("Error while reading input file", stderr);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

const char* Usage
	= "base2 - Wunkolo <wunkolo@gmail.com>\n"
	  "Usage: base2 [Options]... [File]\n"
	  "       base2 --decode [Options]... [File]\n"
	  "Options:\n"
	  "  -h, --help            Display this help/usage information\n"
	  "  -d, --decode          Decodes incoming binary ascii into bytes\n"
	  "  -i, --ignore-garbage  When decoding, ignores non-ascii-binary `0`, "
	  "`1` bytes\n"
	  "  -w, --wrap=Columns    Wrap encoded binary output within columns\n"
	  "                        Default is `76`. `0` Disables linewrapping\n";

const static struct option CommandOptions[5]
	= {{"decode", optional_argument, nullptr, 'd'},
	   {"ignore-garbage", optional_argument, nullptr, 'i'},
	   {"wrap", optional_argument, nullptr, 'w'},
	   {"help", optional_argument, nullptr, 'h'},
	   {nullptr, no_argument, nullptr, '\0'}};

int main(int argc, char* argv[])
{
	Settings CurSettings = {};
	int      Opt;
	int      OptionIndex;
	while( (Opt = getopt_long(argc, argv, "hdiw:", CommandOptions, &OptionIndex)
		   )
		   != -1 )
	{
		switch( Opt )
		{
		case 'd':
			CurSettings.Decode = true;
			break;
		case 'i':
			CurSettings.IgnoreInvalid = true;
			break;
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
		if( std::strcmp(argv[optind], "-") != 0 )
		{
			CurSettings.InputFile = fopen(argv[optind], "rb");
			if( CurSettings.InputFile == nullptr )
			{
				std::fprintf(
					stderr, "Error opening input file: %s\n", argv[optind]
				);
			}
		}
	}
	return (CurSettings.Decode ? Decode : Encode)(CurSettings);
}
