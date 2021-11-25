#include "stringhelpers.hpp"

#include <gtest/gtest.h>

#include "cct_string.hpp"

namespace cct {
TEST(ToChar, Zero) {
  string s;
  AppendChars(s, 0);
  EXPECT_EQ(s, "0");
  SetChars(s, 0);
  EXPECT_EQ(s, "0");
  EXPECT_EQ(ToString<string>(0), "0");
}

TEST(ToChar, PositiveValue) {
  string s("I am a string ");
  AppendChars(s, 42);
  EXPECT_EQ(s, "I am a string 42");
  AppendChars(s, 9);
  EXPECT_EQ(s, "I am a string 429");
  SetChars(s, 902);
  EXPECT_EQ(s, "902");
  EXPECT_EQ(ToString<string>(98124), "98124");
}

TEST(ToChar, NegativeValue) {
  string s("I will hold some negative value ");
  AppendChars(s, -293486);
  EXPECT_EQ(s, "I will hold some negative value -293486");
  AppendChars(s, -9830346445);
  EXPECT_EQ(s, "I will hold some negative value -293486-9830346445");
  SetChars(s, -13);
  EXPECT_EQ(s, "-13");
  EXPECT_EQ(ToString<string>(-123467), "-123467");
}

TEST(ToChar, UnsignedValue) {
  string s("I am a string ");
  AppendChars(s, 738U);
  EXPECT_EQ(s, "I am a string 738");
  AppendChars(s, std::numeric_limits<uint64_t>::max());
  EXPECT_EQ(s, "I am a string 73818446744073709551615");
  SetChars(s, 901235U);
  EXPECT_EQ(s, "901235");
  EXPECT_EQ(ToString<string>(630195439576U), "630195439576");
}
}  // namespace cct