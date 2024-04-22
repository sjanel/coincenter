#include "url-encode.hpp"

#include <gtest/gtest.h>

#include <string_view>

#include "cct_cctype.hpp"

namespace cct {

TEST(URLEncode, Test1) {
  auto isNotEncoded = [](char ch) { return isalnum(ch) || ch == '-' || ch == '.' || ch == '_' || ch == '~'; };

  EXPECT_EQ(URLEncode(std::string_view("2023-05-14T09:42:00"), isNotEncoded), "2023-05-14T09%3A42%3A00");
  EXPECT_EQ(URLEncode(std::string_view("GORgnlPotuGH5cv4JK8d63JWQqkyCPyIo/Z09DvPd4g="), isNotEncoded),
            "GORgnlPotuGH5cv4JK8d63JWQqkyCPyIo%2FZ09DvPd4g%3D");
}

}  // namespace cct