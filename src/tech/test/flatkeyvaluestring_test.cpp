#include "flatkeyvaluestring.hpp"

#include <gtest/gtest.h>

#include "cct_exception.hpp"

namespace cct {
namespace {
using KvPairs = FlatKeyValueString<'&', '='>;
}

TEST(CurlOptionsTest, DefaultConstructor) {
  KvPairs kvPairs;
  EXPECT_TRUE(kvPairs.empty());
  EXPECT_EQ(kvPairs.str(), "");
}

TEST(CurlOptionsTest, EmptyIterator) {
  KvPairs kvPairs;
  EXPECT_EQ(kvPairs.begin(), kvPairs.end());
}

TEST(CurlOptionsTest, SetEmpty) {
  KvPairs kvPairs;
  kvPairs.set("timestamp", "1621785125200");
  EXPECT_EQ(kvPairs.str(), "timestamp=1621785125200");
}

TEST(CurlOptionsTest, SetAndAppend) {
  KvPairs kvPairs;
  kvPairs.append("abc", "666");
  kvPairs.append("de", "aX");
  EXPECT_EQ(kvPairs.get("def"), "");
  EXPECT_FALSE(kvPairs.empty());
  EXPECT_EQ(kvPairs.str(), "abc=666&de=aX");
  kvPairs.set("abc", 777);
  EXPECT_EQ(kvPairs.str(), "abc=777&de=aX");
  kvPairs.set("def", "toto");
  EXPECT_EQ(kvPairs.str(), "abc=777&de=aX&def=toto");
  kvPairs.set("def", "titi");
  EXPECT_EQ(kvPairs.str(), "abc=777&de=aX&def=titi");
  EXPECT_EQ(kvPairs.get("def"), "titi");
  kvPairs.set("777", "yoplalepiege");
  EXPECT_TRUE(kvPairs.contains("777"));
  EXPECT_FALSE(kvPairs.contains("77"));
  EXPECT_EQ(kvPairs.str(), "abc=777&de=aX&def=titi&777=yoplalepiege");
  kvPairs.set("d", "encoreplustricky");
  EXPECT_EQ(kvPairs.str(), "abc=777&de=aX&def=titi&777=yoplalepiege&d=encoreplustricky");
  kvPairs.set("d", "cestboncestfini");
  EXPECT_EQ(kvPairs.str(), "abc=777&de=aX&def=titi&777=yoplalepiege&d=cestboncestfini");
  EXPECT_DEBUG_DEATH(kvPairs.append("777", "cestinterditca"), "");
}

TEST(CurlOptionsTest, Erase) {
  KvPairs kvPairs{{"abc", "354"}, {"tata", "abc"}, {"rm", "xX"}, {"huhu", "haha"}};
  kvPairs.erase("rm");
  const std::string_view expected = "abc=354&tata=abc&huhu=haha";
  EXPECT_EQ(kvPairs.str(), expected);
  kvPairs.erase("haha");
  EXPECT_EQ(kvPairs.str(), expected);
}

TEST(CurlOptionsTest, EmptyConvertToJson) { EXPECT_EQ(KvPairs().toJson(), json()); }

class CurlOptionsCase1 : public ::testing::Test {
 protected:
  virtual void SetUp() {}
  virtual void TearDown() {}

  KvPairs kvPairs{{"units", "0.11176"}, {"price", "357.78"}, {"777", "encoredutravail?"}, {"hola", "quetal"}};
};

TEST_F(CurlOptionsCase1, Get) {
  EXPECT_EQ(kvPairs.get("units"), "0.11176");
  EXPECT_EQ(kvPairs.get("price"), "357.78");
  EXPECT_EQ(kvPairs.get("777"), "encoredutravail?");
  EXPECT_EQ(kvPairs.get("hola"), "quetal");
  EXPECT_EQ(kvPairs.get("laipas"), "");
}

TEST_F(CurlOptionsCase1, Iterator) {
  int i = 0;
  for (const auto &[k, v] : kvPairs) {
    switch (i) {
      case 0:
        EXPECT_EQ(k, "units");
        EXPECT_EQ(v, "0.11176");
        break;
      case 1:
        EXPECT_EQ(k, "price");
        EXPECT_EQ(v, "357.78");
        break;
      case 2:
        EXPECT_EQ(k, "777");
        EXPECT_EQ(v, "encoredutravail?");
        break;
      case 3:
        EXPECT_EQ(k, "hola");
        EXPECT_EQ(v, "quetal");
        break;
    }
    ++i;
  }
  EXPECT_EQ(i, 4);
}

TEST_F(CurlOptionsCase1, ConvertToJson) {
  json jsonData = kvPairs.toJson();

  EXPECT_EQ(jsonData["units"].get<std::string_view>(), "0.11176");
  EXPECT_EQ(jsonData["price"].get<std::string_view>(), "357.78");
  EXPECT_EQ(jsonData["777"].get<std::string_view>(), "encoredutravail?");
  EXPECT_EQ(jsonData["hola"].get<std::string_view>(), "quetal");
}

TEST_F(CurlOptionsCase1, AppendIntegralValues) {
  kvPairs.append("price1", 1957386078376L);
  EXPECT_EQ(kvPairs.get("price1"), "1957386078376");
  int8_t s = -116;
  kvPairs.append("testu", s);
  EXPECT_EQ(kvPairs.get("testu"), "-116");
}

TEST_F(CurlOptionsCase1, SetIntegralValues) {
  kvPairs.set("price1", 42);
  EXPECT_EQ(kvPairs.get("price"), "357.78");
  EXPECT_EQ(kvPairs.get("price1"), "42");
  kvPairs.set("777", -666);
  EXPECT_EQ(kvPairs.get("777"), "-666");
  EXPECT_EQ(kvPairs.str(), "units=0.11176&price=357.78&777=-666&hola=quetal&price1=42");
  int8_t s = -116;
  kvPairs.set("testu", s);
  EXPECT_EQ(kvPairs.get("testu"), "-116");
}

}  // namespace cct