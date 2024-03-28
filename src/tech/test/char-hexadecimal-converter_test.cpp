#include "char-hexadecimal-converter.hpp"

#include <gtest/gtest.h>

#include <string_view>

namespace cct {

class CharHexadecimalConverterTest : public ::testing::Test {
 protected:
  constexpr std::string_view ToUpperHex(char ch) { return {buf, to_upper_hex(ch, buf)}; }

  constexpr std::string_view ToLowerHex(char ch) { return {buf, to_lower_hex(ch, buf)}; }

  char buf[2];
};

TEST_F(CharHexadecimalConverterTest, StandardAscii) {
  EXPECT_EQ(ToUpperHex(0), "00");
  EXPECT_EQ(ToUpperHex(1), "01");
  EXPECT_EQ(ToUpperHex(2), "02");

  EXPECT_EQ(ToUpperHex(11), "0B");

  EXPECT_EQ(ToUpperHex(','), "2C");
  EXPECT_EQ(ToUpperHex('?'), "3F");

  EXPECT_EQ(ToUpperHex('^'), "5E");

  EXPECT_EQ(ToUpperHex('a'), "61");

  EXPECT_EQ(ToUpperHex(127), "7F");
}

TEST_F(CharHexadecimalConverterTest, ExtendedAscii) {
  EXPECT_EQ(ToUpperHex(static_cast<char>(128)), "80");
  EXPECT_EQ(ToUpperHex(static_cast<char>(129)), "81");

  EXPECT_EQ(ToUpperHex(static_cast<char>(155)), "9B");
  EXPECT_EQ(ToUpperHex(static_cast<char>(169)), "A9");

  EXPECT_EQ(ToUpperHex(static_cast<char>(255)), "FF");
}

TEST_F(CharHexadecimalConverterTest, ToLowerHex) {
  EXPECT_EQ(ToLowerHex(static_cast<char>(128)), "80");
  EXPECT_EQ(ToLowerHex(static_cast<char>(129)), "81");

  EXPECT_EQ(ToLowerHex(static_cast<char>(155)), "9b");
  EXPECT_EQ(ToLowerHex(static_cast<char>(169)), "a9");

  EXPECT_EQ(ToLowerHex(static_cast<char>(255)), "ff");
}

}  // namespace cct