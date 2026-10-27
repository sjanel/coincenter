#include "utf8.hpp"

#include <gtest/gtest.h>

#include <cstring>
#include <string_view>

#include "cct_vector.hpp"

namespace cct {

TEST(UTF8Test, ToUTF8) {
  EXPECT_EQ(std::string_view(to_utf8_vector(0x24)), "$");
  EXPECT_EQ(std::string_view(to_utf8_vector(0xA2)), "¢");
  EXPECT_EQ(std::string_view(to_utf8_vector(0x20AC)), "€");
  EXPECT_EQ(std::string_view(to_utf8_vector(0x10348)), "𐍈");
  EXPECT_EQ(std::string_view(to_utf8_vector(0x1F600)), "😀");
}

namespace {
constexpr const char* const kUnicodeStr1 =
    R"(EOS \uc218\ub7c9\uc740 \uc18c\uc218\uc810 8\uc790\ub9ac\uae4c\uc9c0\ub9cc )"
    R"(\uc720\ud6a8\ud569\ub2c8\ub2e4.)";
constexpr const char* const kExpectedStr1 = "EOS 수량은 소수점 8자리까지만 유효합니다.";

constexpr const char* const kUnicodeStr2 =
    R"(\ucd5c\uc18c \uc8fc\ubb38\uae08\uc561\uc740 5000 KRW \uc785\ub2c8\ub2e4.)";
constexpr const char* const kExpectedStr2 = "최소 주문금액은 5000 KRW 입니다.";

}  // namespace

TEST(UTF8Test, DecodeUTF8Str) {
  vector<char> str(kUnicodeStr1, kUnicodeStr1 + std::strlen(kUnicodeStr1));
  decode_utf8(str);
  EXPECT_EQ(std::string_view(str), kExpectedStr1);

  str = vector<char>(kUnicodeStr2, kUnicodeStr2 + std::strlen(kUnicodeStr2));
  decode_utf8(str);
  EXPECT_EQ(std::string_view(str), kExpectedStr2);
}

TEST(UTF8Test, DecodeUTF8CharArray) {
  vector<char> str(kUnicodeStr1, kUnicodeStr1 + std::strlen(kUnicodeStr1));
  char* newEnd = decode_utf8(str.data(), str.data() + str.size());
  EXPECT_EQ(std::string_view(str.data(), newEnd), kExpectedStr1);

  str = vector<char>(kUnicodeStr2, kUnicodeStr2 + std::strlen(kUnicodeStr2));
  newEnd = decode_utf8(str.data(), str.data() + str.size());
  EXPECT_EQ(std::string_view(str.data(), newEnd), kExpectedStr2);
}

}  // namespace cct