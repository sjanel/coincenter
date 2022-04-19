#include "codec.hpp"

#include <gtest/gtest.h>

namespace cct {

TEST(Base64, EncodeEmpty) { EXPECT_EQ(B64Encode(""), ""); }
TEST(Base64, Encode1) { EXPECT_EQ(B64Encode("f"), "Zg=="); }
TEST(Base64, Encode2) { EXPECT_EQ(B64Encode("fo"), "Zm8="); }
TEST(Base64, Encode3) { EXPECT_EQ(B64Encode("foo"), "Zm9v"); }
TEST(Base64, Encode4) { EXPECT_EQ(B64Encode("foob"), "Zm9vYg=="); }
TEST(Base64, Encode5) { EXPECT_EQ(B64Encode("fooba"), "Zm9vYmE="); }
TEST(Base64, Encode6) { EXPECT_EQ(B64Encode("foobar"), "Zm9vYmFy"); }

TEST(Base64, DecodeEmpty) { EXPECT_EQ(B64Decode(""), ""); }
TEST(Base64, Decode1) { EXPECT_EQ(B64Decode("Zg=="), "f"); }
TEST(Base64, Decode2) { EXPECT_EQ(B64Decode("Zm8="), "fo"); }
TEST(Base64, Decode3) { EXPECT_EQ(B64Decode("Zm9v"), "foo"); }
TEST(Base64, Decode4) { EXPECT_EQ(B64Decode("Zm9vYg=="), "foob"); }
TEST(Base64, Decode5) { EXPECT_EQ(B64Decode("Zm9vYmE="), "fooba"); }
TEST(Base64, Decode6) { EXPECT_EQ(B64Decode("Zm9vYmFy"), "foobar"); }

}  // namespace cct
