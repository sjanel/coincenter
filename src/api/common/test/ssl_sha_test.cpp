
#include "ssl_sha.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <iterator>

#include "cct_string.hpp"

namespace cct::ssl {
TEST(SSLTest, Version) { EXPECT_NE(GetOpenSSLVersion(), ""); }

TEST(SSLTest, AppendSha256) {
  string str("test");
  AppendSha256("thisNonce0123456789Data", str);

  static constexpr char kExpectedData[] = {116,  101,  115, 116,  -98,  74,   -90, 56, -41, 61,   -33, 98,
                                           -108, -110, -41, -82,  -110, -102, -80, 85, 127, -112, -55, -116,
                                           38,   36,   10,  -104, -37,  93,   105, 14, 73,  99,   98,  95};
  EXPECT_TRUE(std::equal(str.begin(), str.end(), std::begin(kExpectedData), std::end(kExpectedData)));
}

TEST(SSLTest, ShaBin256) {
  auto actual = ShaBin(ShaType::kSha256, "data1234", "secret1234");
  static constexpr char kExpectedData[] = {11,  -51, -56,  -21,  -101, 61,   35,  28, 86,  97,  -50,
                                           -8,  47,  -113, -13,  -107, -100, -93, 27, 71,  101, -128,
                                           -65, 101, -110, -123, 38,   73,   77,  73, -10, -39};
  EXPECT_TRUE(std::equal(actual.begin(), actual.end(), std::begin(kExpectedData), std::end(kExpectedData)));
}

TEST(SSLTest, ShaBin512) {
  auto actual = ShaBin(ShaType::kSha512, "data1234", "secret1234");
  static constexpr char kExpectedData[] = {-22, 39,  95,   -39, -44,  39,  97,   -40, 29,   -120, -125, 84,  -112,
                                           -5,  69,  -111, 3,   -109, 86,  54,   -31, 44,   -55,  56,   111, 85,
                                           87,  22,  -61,  82,  89,   52,  105,  2,   -89,  -76,  63,   4,   95,
                                           124, -24, 93,   -46, -104, -87, -110, -80, -77,  -66,  -43,  26,  126,
                                           114, 101, -68,  -66, 75,   -62, -93,  -77, -124, 78,   -121, -96};
  EXPECT_TRUE(std::equal(actual.begin(), actual.end(), std::begin(kExpectedData), std::end(kExpectedData)));
}

TEST(SSLTest, ShaHex256) {
  auto actual = ShaHex(ShaType::kSha256, "data1234", "secret1234");
  static constexpr char kExpectedData[] = {48, 98, 99, 100, 99, 56,  101, 98, 57, 98,  51, 100, 50,  51,  49,  99,
                                           53, 54, 54, 49,  99, 101, 102, 56, 50, 102, 56, 102, 102, 51,  57,  53,
                                           57, 99, 97, 51,  49, 98,  52,  55, 54, 53,  56, 48,  98,  102, 54,  53,
                                           57, 50, 56, 53,  50, 54,  52,  57, 52, 100, 52, 57,  102, 54,  100, 57};
  EXPECT_TRUE(std::equal(actual.begin(), actual.end(), std::begin(kExpectedData), std::end(kExpectedData)));
}

TEST(SSLTest, ShaHex512) {
  auto actual = ShaHex(ShaType::kSha512, "data1234", "secret1234");
  static constexpr char kExpectedData[] = {
      101, 97,  50, 55, 53,  102, 100, 57,  100, 52,  50,  55,  54, 49,  100, 56,  49, 100, 56,  56, 56, 51,
      53,  52,  57, 48, 102, 98,  52,  53,  57,  49,  48,  51,  57, 51,  53,  54,  51, 54,  101, 49, 50, 99,
      99,  57,  51, 56, 54,  102, 53,  53,  53,  55,  49,  54,  99, 51,  53,  50,  53, 57,  51,  52, 54, 57,
      48,  50,  97, 55, 98,  52,  51,  102, 48,  52,  53,  102, 55, 99,  101, 56,  53, 100, 100, 50, 57, 56,
      97,  57,  57, 50, 98,  48,  98,  51,  98,  101, 100, 53,  49, 97,  55,  101, 55, 50,  54,  53, 98, 99,
      98,  101, 52, 98, 99,  50,  97,  51,  98,  51,  56,  52,  52, 101, 56,  55,  97, 48};
  EXPECT_TRUE(std::equal(actual.begin(), actual.end(), std::begin(kExpectedData), std::end(kExpectedData)));
}

TEST(SSLTest, ShaDigest256) {
  auto actual = ShaDigest(ShaType::kSha256, "data1234");
  static constexpr char kExpectedData[] = {102, 50, 102, 100, 97,  57, 98, 98,  53, 49,  49,  56,  100, 100, 53, 97,
                                           51,  50, 57,  55,  100, 50, 56, 97,  52, 55,  50,  57,  51,  102, 49, 50,
                                           51,  97, 49,  51,  50,  54, 50, 57,  48, 101, 102, 51,  100, 55,  48, 49,
                                           53,  57, 55,  101, 57,  51, 56, 102, 99, 53,  48,  102, 48,  57,  57, 57};
  EXPECT_TRUE(std::equal(actual.begin(), actual.end(), std::begin(kExpectedData), std::end(kExpectedData)));
}

TEST(SSLTest, ShaDigest512) {
  auto actual = ShaDigest(ShaType::kSha512, "data1234");
  static constexpr char kExpectedData[] = {
      99,  97,  97,  48,  50,  55, 54,  50,  57, 52,  98,  54,  49, 53,  48,  50,  51, 100, 57,  55,  50, 54,
      48,  52,  55,  53,  50,  54, 98,  49,  50, 52,  102, 100, 99, 100, 51,  49,  98, 100, 97,  101, 97, 56,
      100, 102, 54,  54,  101, 48, 101, 54,  97, 101, 101, 102, 48, 101, 48,  102, 57, 54,  100, 54,  52, 55,
      50,  49,  99,  100, 100, 57, 54,  102, 50, 52,  48,  54,  48, 102, 101, 49,  56, 102, 52,  52,  50, 100,
      54,  57,  100, 98,  54,  99, 56,  53,  56, 49,  53,  51,  52, 57,  102, 50,  52, 101, 99,  100, 99, 48,
      100, 97,  51,  51,  51,  53, 48,  49,  56, 51,  53,  98,  53, 52,  51,  102, 54, 53};
  EXPECT_TRUE(std::equal(actual.begin(), actual.end(), std::begin(kExpectedData), std::end(kExpectedData)));
}

TEST(SSLTest, ShaDigest256Multiple) {
  static const string kData[] = {"data1234", "anotherString5_-", "5_0(7)fbBBBb334G;"};

  auto actual = ShaDigest(ShaType::kSha256, kData);
  static constexpr char kExpectedData[] = {53,  53, 100, 98, 52,  97,  49, 97,  50,  99, 52, 52, 52, 99,  97, 57,
                                           100, 57, 97,  52, 48,  99,  51, 52,  101, 97, 50, 99, 53, 98,  97, 51,
                                           100, 54, 55,  50, 102, 100, 51, 102, 100, 98, 51, 54, 52, 100, 98, 50,
                                           101, 49, 97,  56, 53,  54,  99, 48,  100, 53, 52, 98, 49, 101, 51, 50};
  EXPECT_TRUE(std::equal(actual.begin(), actual.end(), std::begin(kExpectedData), std::end(kExpectedData)));
}

TEST(SSLTest, ShaDigest512Multiple) {
  static const string kData[] = {"data1234", "anotherString5_-", "5_0(7)fbBBBb334G;"};

  auto actual = ShaDigest(ShaType::kSha512, kData);
  static constexpr char kExpectedData[] = {
      101, 56,  101, 55,  54,  98,  100, 56, 57, 53, 100, 53, 54, 99,  50,  54,  48, 56,  50, 57,  53,  97,
      100, 98,  100, 48,  55,  102, 51,  56, 49, 54, 99,  55, 99, 101, 101, 98,  54, 48,  53, 52,  100, 98,
      54,  98,  56,  102, 97,  52,  51,  57, 48, 56, 102, 54, 54, 48,  100, 49,  57, 101, 98, 51,  99,  55,
      48,  56,  54,  49,  100, 57,  49,  51, 98, 97, 57,  53, 56, 54,  54,  54,  52, 53,  53, 48,  57,  98,
      100, 99,  102, 57,  54,  52,  55,  55, 49, 55, 48,  56, 97, 55,  50,  102, 52, 54,  52, 101, 56,  50,
      51,  102, 57,  54,  98,  53,  50,  51, 52, 99, 98,  48, 51, 56,  100, 53,  55, 56};
  EXPECT_TRUE(std::equal(actual.begin(), actual.end(), std::begin(kExpectedData), std::end(kExpectedData)));
}
}  // namespace cct::ssl