#include "curloptions.hpp"

#include <gtest/gtest.h>

#include "cct_exception.hpp"

namespace cct {
TEST(CurlOptionsTest, DefaultConstructor) {
  CurlPostData curlPostData;
  EXPECT_TRUE(curlPostData.empty());
  EXPECT_STREQ(curlPostData.c_str(), "");
}

TEST(CurlOptionsTest, Main) {
  CurlPostData curlPostData;
  curlPostData.append("abc", "666");
  curlPostData.append("de", "aX");
  EXPECT_EQ(curlPostData.get("def"), "");
  EXPECT_FALSE(curlPostData.empty());
  EXPECT_STREQ(curlPostData.c_str(), "abc=666&de=aX");
  curlPostData.set("abc", 777);
  EXPECT_STREQ(curlPostData.c_str(), "abc=777&de=aX");
  curlPostData.set("def", "toto");
  EXPECT_STREQ(curlPostData.c_str(), "abc=777&de=aX&def=toto");
  curlPostData.set("def", "titi");
  EXPECT_STREQ(curlPostData.c_str(), "abc=777&de=aX&def=titi");
  EXPECT_EQ(curlPostData.get("def"), "titi");
  curlPostData.set("777", "yoplalepiege");
  EXPECT_TRUE(curlPostData.contains("777"));
  EXPECT_FALSE(curlPostData.contains("77"));
  EXPECT_STREQ(curlPostData.c_str(), "abc=777&de=aX&def=titi&777=yoplalepiege");
  curlPostData.set("d", "encoreplustricky");
  EXPECT_STREQ(curlPostData.c_str(), "abc=777&de=aX&def=titi&777=yoplalepiege&d=encoreplustricky");
  curlPostData.set("d", "cestboncestfini");
  EXPECT_STREQ(curlPostData.c_str(), "abc=777&de=aX&def=titi&777=yoplalepiege&d=cestboncestfini");
  EXPECT_THROW(curlPostData.append("777", "cestinterditca"), cct::exception);
}

TEST(CurlOptionsTest, Erase) {
  CurlPostData curlPostData{{"abc", "354"}, {"tata", "abc"}, {"rm", "xX"}, {"huhu", "haha"}};
  curlPostData.erase("rm");
  const std::string_view expected = "abc=354&tata=abc&huhu=haha";
  EXPECT_EQ(curlPostData.toStringView(), expected);
  curlPostData.erase("haha");
  EXPECT_EQ(curlPostData.toStringView(), expected);
}

TEST(CurlOptionsTest, Get) {
  CurlPostData curlPostData{{"units", "0.11176"}, {"price", "357.78"}, {"777", "encoredutravail?"}, {"hola", "quetal"}};
  EXPECT_EQ(curlPostData.get("units"), "0.11176");
  EXPECT_EQ(curlPostData.get("price"), "357.78");
  EXPECT_EQ(curlPostData.get("777"), "encoredutravail?");
  EXPECT_EQ(curlPostData.get("hola"), "quetal");
  EXPECT_EQ(curlPostData.get("laipas"), "");
}
}  // namespace cct