#pragma once
#include <cstdint>
#include <cstddef>

namespace Base2
{

void Encode(
	const std::uint8_t Input[], std::uint64_t Output[], std::size_t Length
);

void Decode(
	const std::uint64_t Input[], std::uint8_t Output[], std::size_t Length
);

}
