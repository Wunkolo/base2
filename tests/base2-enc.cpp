#include <Base2.hpp>

#include <algorithm>
#include <string>
#include <string_view>

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
  std::string Output;
  Output.resize(8);

  Base2::Encode(&Input, reinterpret_cast<std::uint64_t *>(Output.data()), 1);

  REQUIRE(Output == "00000000");
}

TEST_CASE("One", "[Base2]") {
  std::uint8_t Input = 1;
  std::string Output;
  Output.resize(8);

  Base2::Encode(&Input, reinterpret_cast<std::uint64_t *>(Output.data()), 1);

  REQUIRE(Output == "00000001");
}

TEST_CASE("Zeros x7", "[Base2]") {
  std::vector<std::uint8_t> Input(7, 0);
  std::string Output;
  Output.resize(Input.size() * 8);

  Base2::Encode(Input.data(), reinterpret_cast<std::uint64_t *>(Output.data()),
                Input.size());

  const std::string_view OutputView(Output);
  for (std::size_t i = 0; i < OutputView.size(); i += 8) {
    const std::string_view CurByte = OutputView.substr(i, 8);
    REQUIRE(CurByte == "00000000");
  }
}

TEST_CASE("Ones x7", "[Base2]") {
  std::vector<std::uint8_t> Input(7, 1);
  std::string Output;
  Output.resize(Input.size() * 8);

  Base2::Encode(Input.data(), reinterpret_cast<std::uint64_t *>(Output.data()),
                Input.size());

  const std::string_view OutputView(Output);
  for (std::size_t i = 0; i < OutputView.size(); i += 8) {
    const std::string_view CurByte = OutputView.substr(i, 8);
    REQUIRE(CurByte == "00000001");
  }
}

TEST_CASE("Zeros x713", "[Base2]") {
  std::vector<std::uint8_t> Input(713, 0);
  std::string Output;
  Output.resize(Input.size() * 8);

  Base2::Encode(Input.data(), reinterpret_cast<std::uint64_t *>(Output.data()),
                Input.size());

  const std::string_view OutputView(Output);
  for (std::size_t i = 0; i < OutputView.size(); i += 8) {
    const std::string_view CurByte = OutputView.substr(i, 8);
    REQUIRE(CurByte == "00000000");
  }
}

TEST_CASE("Ones x713", "[Base2]") {
  std::vector<std::uint8_t> Input(713, 1);
  std::string Output;
  Output.resize(Input.size() * 8);

  Base2::Encode(Input.data(), reinterpret_cast<std::uint64_t *>(Output.data()),
                Input.size());

  const std::string_view OutputView(Output);
  for (std::size_t i = 0; i < OutputView.size(); i += 8) {
    const std::string_view CurByte = OutputView.substr(i, 8);
    REQUIRE(CurByte == "00000001");
  }
}

TEST_CASE("Evens x713", "[Base2]") {
  std::vector<std::uint8_t> Input(713);
  std::generate(Input.begin(), Input.end(),
                [i = 0ULL]() mutable { return (++i % 2) ? 0 : 1; });

  std::string Output;
  Output.resize(Input.size() * 8);

  Base2::Encode(Input.data(), reinterpret_cast<std::uint64_t *>(Output.data()),
                Input.size());

  const std::string_view OutputView(Output);
  for (std::size_t i = 0; i < OutputView.size() - 16; i += 16) {
    const std::string_view CurSpan = OutputView.substr(i, 16);
    REQUIRE(CurSpan == "0000000000000001");
  }
}

TEST_CASE("Odds x713", "[Base2]") {
  std::vector<std::uint8_t> Input(713);
  std::generate(Input.begin(), Input.end(),
                [i = 0ULL]() mutable { return (++i % 2) ? 1 : 0; });

  std::string Output;
  Output.resize(Input.size() * 8);

  Base2::Encode(Input.data(), reinterpret_cast<std::uint64_t *>(Output.data()),
                Input.size());

  const std::string_view OutputView(Output);
  for (std::size_t i = 0; i < OutputView.size() - 16; i += 16) {
    const std::string_view CurSpan = OutputView.substr(i, 16);
    REQUIRE(CurSpan == "0000000100000000");
  }
}