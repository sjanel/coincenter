#include "base64.hpp"

#include <gtest/gtest.h>

#include <span>
#include <string_view>

namespace cct {

TEST(Base64, EncodeEmpty) { EXPECT_EQ(B64Encode(std::string_view("")), ""); }
TEST(Base64, Encode1) { EXPECT_EQ(B64Encode(std::string_view("f")), "Zg=="); }
TEST(Base64, Encode2) { EXPECT_EQ(B64Encode(std::string_view("fo")), "Zm8="); }
TEST(Base64, Encode3) { EXPECT_EQ(B64Encode(std::string_view("foo")), "Zm9v"); }
TEST(Base64, Encode4) { EXPECT_EQ(B64Encode(std::string_view("foob")), "Zm9vYg=="); }
TEST(Base64, Encode5) { EXPECT_EQ(B64Encode(std::string_view("fooba")), "Zm9vYmE="); }
TEST(Base64, Encode6) { EXPECT_EQ(B64Encode(std::string_view("foobar")), "Zm9vYmFy"); }
TEST(Base64, Encode7) { EXPECT_EQ(B64Encode(std::string_view("foobarz")), "Zm9vYmFyeg=="); }
TEST(Base64, Encode8) { EXPECT_EQ(B64Encode(std::string_view("foobarzY")), "Zm9vYmFyelk="); }
TEST(Base64, Encode9) { EXPECT_EQ(B64Encode(std::string_view("foobarzYg")), "Zm9vYmFyelln"); }

TEST(Base64, DecodeEmpty) { EXPECT_EQ(B64Decode(std::string_view("")), ""); }
TEST(Base64, Decode1) { EXPECT_EQ(B64Decode(std::string_view("Zg==")), "f"); }
TEST(Base64, Decode2) { EXPECT_EQ(B64Decode(std::string_view("Zm8=")), "fo"); }
TEST(Base64, Decode3) { EXPECT_EQ(B64Decode(std::string_view("Zm9v")), "foo"); }
TEST(Base64, Decode4) { EXPECT_EQ(B64Decode(std::string_view("Zm9vYg==")), "foob"); }
TEST(Base64, Decode5) { EXPECT_EQ(B64Decode(std::string_view("Zm9vYmE=")), "fooba"); }
TEST(Base64, Decode6) { EXPECT_EQ(B64Decode(std::string_view("Zm9vYmFy")), "foobar"); }
TEST(Base64, Decode7) { EXPECT_EQ(B64Decode(std::string_view("Zm9vYmFyeg==")), "foobarz"); }
TEST(Base64, Decode8) { EXPECT_EQ(B64Decode(std::string_view("Zm9vYmFyelk=")), "foobarzY"); }
TEST(Base64, Decode9) { EXPECT_EQ(B64Decode(std::string_view("Zm9vYmFyelln")), "foobarzYg"); }

}  // namespace cct
