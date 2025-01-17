#include <Base2.hpp>

#include <algorithm>
#include <array>
#include <string>

#include <catch2/catch_test_macros.hpp>

static std::string TestEncode(std::string Input) {
  std::string Output;
  Output.resize(Input.size() * 8);

  Base2::Encode(reinterpret_cast<const std::uint8_t *>(Input.data()),
                reinterpret_cast<std::uint64_t *>(Output.data()),
                Input.length());

  return Output;
}

TEST_CASE("Zero", "[Base2]") {
  std::uint8_t Input = 0;
  std::array<char, 8> Output;

  Base2::Encode(&Input, reinterpret_cast<std::uint64_t *>(Output.data()), 1);

  REQUIRE(std::memcmp(Output.data(), u8"00000000", 8) == 0);
}