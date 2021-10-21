#include "curloptions.hpp"

#include <gtest/gtest.h>

#include "cct_exception.hpp"

namespace cct {
TEST(CurlOptionsTest, DefaultConstructor) {
  CurlPostData curlPostData;
  EXPECT_TRUE(curlPostData.empty());
  EXPECT_STREQ(curlPostData.c_str(), "");
}

TEST(CurlOptionsTest, SetEmpty) {
  CurlPostData curlPostData;
  curlPostData.set("timestamp", "1621785125200");
  EXPECT_STREQ(curlPostData.c_str(), "timestamp=1621785125200");
}

TEST(CurlOptionsTest, SetAndAppend) {
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
  EXPECT_THROW(curlPostData.append("777", "cestinterditca"), exception);
}

TEST(CurlOptionsTest, Erase) {
  CurlPostData curlPostData{{"abc", "354"}, {"tata", "abc"}, {"rm", "xX"}, {"huhu", "haha"}};
  curlPostData.erase("rm");
  const std::string_view expected = "abc=354&tata=abc&huhu=haha";
  EXPECT_EQ(curlPostData.str(), expected);
  curlPostData.erase("haha");
  EXPECT_EQ(curlPostData.str(), expected);
}

TEST(CurlOptionsTest, EmptyConvertToJson) {
  json emptyJson = CurlPostData().toJson();
  EXPECT_EQ(emptyJson, json());
}

class CurlOptionsCase1 : public ::testing::Test {
 protected:
  virtual void SetUp() {}
  virtual void TearDown() {}

  CurlPostData curlPostData{{"units", "0.11176"}, {"price", "357.78"}, {"777", "encoredutravail?"}, {"hola", "quetal"}};
};

TEST_F(CurlOptionsCase1, Get) {
  EXPECT_EQ(curlPostData.get("units"), "0.11176");
  EXPECT_EQ(curlPostData.get("price"), "357.78");
  EXPECT_EQ(curlPostData.get("777"), "encoredutravail?");
  EXPECT_EQ(curlPostData.get("hola"), "quetal");
  EXPECT_EQ(curlPostData.get("laipas"), "");
}

TEST_F(CurlOptionsCase1, ConvertToJson) {
  json jsonData = curlPostData.toJson();

  EXPECT_EQ(jsonData["units"].get<std::string_view>(), "0.11176");
  EXPECT_EQ(jsonData["price"].get<std::string_view>(), "357.78");
  EXPECT_EQ(jsonData["777"].get<std::string_view>(), "encoredutravail?");
  EXPECT_EQ(jsonData["hola"].get<std::string_view>(), "quetal");
}

TEST_F(CurlOptionsCase1, AppendIntegralValues) {
  curlPostData.append("price1", 1957386078376L);
  EXPECT_EQ(curlPostData.get("price1"), "1957386078376");
  int8_t s = -116;
  curlPostData.append("testu", s);
  EXPECT_EQ(curlPostData.get("testu"), "-116");
}

TEST_F(CurlOptionsCase1, SetIntegralValues) {
  curlPostData.set("price1", 42);
  EXPECT_EQ(curlPostData.get("price"), "357.78");
  EXPECT_EQ(curlPostData.get("price1"), "42");
  curlPostData.set("777", -666);
  EXPECT_EQ(curlPostData.get("777"), "-666");
  EXPECT_EQ(curlPostData.str(), "units=0.11176&price=357.78&777=-666&hola=quetal&price1=42");
  int8_t s = -116;
  curlPostData.set("testu", s);
  EXPECT_EQ(curlPostData.get("testu"), "-116");
}

}  // namespace cct