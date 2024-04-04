#include "stringhelpers.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

#include "cct_string.hpp"

namespace cct {
TEST(ToChar, Zero) {
  string str;
  AppendString(str, 0);
  EXPECT_EQ(str, "0");
  SetString(str, 0);
  EXPECT_EQ(str, "0");
  EXPECT_EQ(ToString(0), "0");
}

TEST(ToChar, PositiveValue) {
  string str("I am a string ");
  AppendString(str, 42);
  EXPECT_EQ(str, "I am a string 42");
  AppendString(str, 9);
  EXPECT_EQ(str, "I am a string 429");
  SetString(str, 902);
  EXPECT_EQ(str, "902");
  EXPECT_EQ(ToString(98124), "98124");
}

TEST(ToChar, NegativeValue) {
  string str("I will hold some negative value ");
  AppendString(str, -293486);
  EXPECT_EQ(str, "I will hold some negative value -293486");
  AppendString(str, -9830346445);
  EXPECT_EQ(str, "I will hold some negative value -293486-9830346445");
  SetString(str, -13);
  EXPECT_EQ(str, "-13");
  EXPECT_EQ(ToString(-123467), "-123467");
}

TEST(ToChar, UnsignedValue) {
  string str("I am a string ");
  AppendString(str, 738U);
  EXPECT_EQ(str, "I am a string 738");
  AppendString(str, std::numeric_limits<uint64_t>::max());
  EXPECT_EQ(str, "I am a string 73818446744073709551615");
  SetString(str, 901235U);
  EXPECT_EQ(str, "901235");
  EXPECT_EQ(ToString(630195439576U), "630195439576");
}

TEST(ToCharVector, PositiveValueInt8) { EXPECT_EQ(std::string_view(ToCharVector(static_cast<int8_t>(3))), "3"); }

TEST(ToCharVector, NegativeValueInt8) { EXPECT_EQ(std::string_view(ToCharVector(static_cast<int8_t>(-11))), "-11"); }

TEST(ToCharVector, PositiveValueInt) { EXPECT_EQ(std::string_view(ToCharVector(34)), "34"); }

TEST(ToCharVector, NegativeValueInt16) {
  EXPECT_EQ(std::string_view(ToCharVector(static_cast<int16_t>(-31678))), "-31678");
}

TEST(ToCharVector, PositiveValueUint64) {
  EXPECT_EQ(std::string_view(ToCharVector(std::numeric_limits<uint64_t>::max())), "18446744073709551615");
}

TEST(FromString, PositiveValue) {
  EXPECT_EQ(FromString<int>("0"), 0);
  EXPECT_EQ(FromString<int>("00"), 0);
  EXPECT_EQ(FromString<int>("036"), 36);
  EXPECT_EQ(FromString<int>("9105470"), 9105470);
}

TEST(FromString, NegativeValue) {
  EXPECT_EQ(FromString<int>("-0"), 0);
  EXPECT_EQ(FromString<int>("-00"), 0);
  EXPECT_EQ(FromString<int>("-036"), -36);
  EXPECT_EQ(FromString<int>("-9105470"), -9105470);
}

TEST(strnlen, strnlen) {
  EXPECT_EQ(strnlen("123456789", 2), 2);
  EXPECT_EQ(strnlen("123456789", 3), 3);
  EXPECT_EQ(strnlen("123456789", 12), 9);
}

}  // namespace cct