#include "flatkeyvaluestring.hpp"

#include <gtest/gtest.h>

#include "cct_exception.hpp"

namespace cct {
namespace {
using KvPairs = FlatKeyValueString<'&', '='>;
}

TEST(FlatKeyValueStringTest, DefaultConstructor) {
  KvPairs kvPairs;
  EXPECT_TRUE(kvPairs.empty());
  EXPECT_EQ(kvPairs.str(), "");
}

TEST(FlatKeyValueStringTest, EmptyIterator) {
  KvPairs kvPairs;
  EXPECT_EQ(kvPairs.begin(), kvPairs.end());
}

TEST(FlatKeyValueStringTest, SetEmpty) {
  KvPairs kvPairs;
  kvPairs.set("timestamp", "1621785125200");
  EXPECT_EQ(kvPairs.str(), "timestamp=1621785125200");
}

TEST(FlatKeyValueStringTest, SetAndAppend) {
  KvPairs kvPairs;
  kvPairs.append("abc", "666");
  kvPairs.append({"de", "aX"});
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
  EXPECT_DEBUG_DEATH(kvPairs.append("newKey", "="), "");
}

TEST(FlatKeyValueStringTest, Prepend) {
  KvPairs kvPairs;
  kvPairs.prepend("statue", "liberty");
  EXPECT_EQ(kvPairs.str(), "statue=liberty");
  kvPairs.prepend("city", "New York City");
  EXPECT_EQ(kvPairs.str(), "city=New York City&statue=liberty");
  kvPairs.prepend({"state", "New York"});
  EXPECT_EQ(kvPairs.str(), "state=New York&city=New York City&statue=liberty");
  kvPairs.prepend("Postal Code", 10015);
  EXPECT_EQ(kvPairs.str(), "Postal Code=10015&state=New York&city=New York City&statue=liberty");
}

TEST(FlatKeyValueStringTest, Erase) {
  KvPairs kvPairs{{"abc", "354"}, {"tata", "abc"}, {"rm", "xX"}, {"huhu", "haha"}};
  kvPairs.erase("rm");
  EXPECT_EQ(kvPairs.str(), "abc=354&tata=abc&huhu=haha");
  kvPairs.erase("haha");
  EXPECT_EQ(kvPairs.str(), "abc=354&tata=abc&huhu=haha");
  kvPairs.erase("abc");
  EXPECT_EQ(kvPairs.str(), "tata=abc&huhu=haha");
  kvPairs.erase("huhu");
  EXPECT_EQ(kvPairs.str(), "tata=abc");
  kvPairs.erase("abc");
  EXPECT_EQ(kvPairs.str(), "tata=abc");
  kvPairs.erase("tata");
  EXPECT_TRUE(kvPairs.empty());
}

TEST(FlatKeyValueStringTest, WithNullTerminatingCharAsSeparator) {
  using namespace std::literals;

  using ExoticKeyValuePair = FlatKeyValueString<'\0', ':'>;
  ExoticKeyValuePair kvPairs{{"abc", "354"}, {"tata", "abc"}, {"rm", "xX"}, {"huhu", "haha"}};
  EXPECT_EQ(kvPairs.str(), std::string_view("abc:354\0tata:abc\0rm:xX\0huhu:haha"sv));
  kvPairs.set("rm", "Yy3");
  EXPECT_EQ(kvPairs.str(), std::string_view("abc:354\0tata:abc\0rm:Yy3\0huhu:haha"sv));
  kvPairs.erase("abc");
  EXPECT_EQ(kvPairs.str(), std::string_view("tata:abc\0rm:Yy3\0huhu:haha"sv));
  kvPairs.erase("rm");
  EXPECT_EQ(kvPairs.str(), std::string_view("tata:abc\0huhu:haha"sv));
  kvPairs.append("&newField", "&&newValue&&");
  EXPECT_EQ(kvPairs.str(), std::string_view("tata:abc\0huhu:haha\0&newField:&&newValue&&"sv));

  int kvPairPos = 0;
  for (const auto &[key, val] : kvPairs) {
    const char *kvPairPtr = key.data();
    switch (kvPairPos++) {
      case 0:
        ASSERT_STREQ(kvPairPtr, "tata:abc");
        break;
      case 1:
        ASSERT_STREQ(kvPairPtr, "huhu:haha");
        break;
      case 2:
        ASSERT_STREQ(kvPairPtr, "&newField:&&newValue&&");
        break;
    }
  }
  EXPECT_EQ(kvPairPos, 3);
}

TEST(FlatKeyValueStringTest, EmptyConvertToJson) { EXPECT_EQ(KvPairs().toJson(), json()); }

class CurlOptionsCase1 : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}

  KvPairs kvPairs{{"units", "0.11176"}, {"price", "357.78"},  {"777", "encoredutravail?"},
                  {"hola", "quetal"},   {"array1", "val1,,"}, {"array2", ",val1,val2,value,"},
                  {"emptyArray", ","}};
};

TEST_F(CurlOptionsCase1, Get) {
  EXPECT_EQ(kvPairs.get("units"), "0.11176");
  EXPECT_EQ(kvPairs.get("price"), "357.78");
  EXPECT_EQ(kvPairs.get("777"), "encoredutravail?");
  EXPECT_EQ(kvPairs.get("hola"), "quetal");
  EXPECT_EQ(kvPairs.get("array1"), "val1,,");
  EXPECT_EQ(kvPairs.get("array2"), ",val1,val2,value,");
  EXPECT_EQ(kvPairs.get("emptyArray"), ",");

  EXPECT_EQ(kvPairs.get("laipas"), "");
}

TEST_F(CurlOptionsCase1, Iterator) {
  int itPos = 0;
  for (const auto &[key, val] : kvPairs) {
    switch (itPos++) {
      case 0:
        EXPECT_EQ(key, "units");
        EXPECT_EQ(val, "0.11176");
        break;
      case 1:
        EXPECT_EQ(key, "price");
        EXPECT_EQ(val, "357.78");
        break;
      case 2:
        EXPECT_EQ(key, "777");
        EXPECT_EQ(val, "encoredutravail?");
        break;
      case 3:
        EXPECT_EQ(key, "hola");
        EXPECT_EQ(val, "quetal");
        break;
      case 4:
        EXPECT_EQ(key, "array1");
        EXPECT_EQ(val, "val1,,");
        break;
      case 5:
        EXPECT_EQ(key, "array2");
        EXPECT_EQ(val, ",val1,val2,value,");
        break;
      case 6:
        EXPECT_EQ(key, "emptyArray");
        EXPECT_EQ(val, ",");
        break;
    }
  }
  EXPECT_EQ(itPos, 7);
}

TEST_F(CurlOptionsCase1, ConvertToJson) {
  json jsonData = kvPairs.toJson();

  EXPECT_EQ(jsonData["units"].get<std::string_view>(), "0.11176");
  EXPECT_EQ(jsonData["price"].get<std::string_view>(), "357.78");
  EXPECT_EQ(jsonData["777"].get<std::string_view>(), "encoredutravail?");
  EXPECT_EQ(jsonData["hola"].get<std::string_view>(), "quetal");
  EXPECT_FALSE(jsonData["hola"].is_array());

  auto arrayIt = jsonData.find("array1");
  EXPECT_NE(arrayIt, jsonData.end());
  EXPECT_TRUE(arrayIt->is_array());
  EXPECT_EQ(arrayIt->size(), 2U);

  EXPECT_EQ((*arrayIt)[0], "val1");
  EXPECT_EQ((*arrayIt)[1], "");

  arrayIt = jsonData.find("array2");
  EXPECT_NE(arrayIt, jsonData.end());
  EXPECT_TRUE(arrayIt->is_array());
  EXPECT_EQ(arrayIt->size(), 4U);

  EXPECT_EQ((*arrayIt)[0], "");
  EXPECT_EQ((*arrayIt)[1], "val1");
  EXPECT_EQ((*arrayIt)[2], "val2");
  EXPECT_EQ((*arrayIt)[3], "value");

  arrayIt = jsonData.find("emptyArray");
  EXPECT_NE(arrayIt, jsonData.end());
  EXPECT_TRUE(arrayIt->is_array());
  EXPECT_TRUE(arrayIt->empty());
}

TEST_F(CurlOptionsCase1, AppendIntegralValues) {
  kvPairs.append("price1", 1957386078376L);
  EXPECT_EQ(kvPairs.get("price1"), "1957386078376");
  int8_t val = -116;
  kvPairs.append("testu", val);
  EXPECT_EQ(kvPairs.get("testu"), "-116");
}

TEST_F(CurlOptionsCase1, SetIntegralValues) {
  kvPairs.set("price1", 42);
  EXPECT_EQ(kvPairs.get("price"), "357.78");
  EXPECT_EQ(kvPairs.get("price1"), "42");
  kvPairs.set("777", -666);
  EXPECT_EQ(kvPairs.get("777"), "-666");
  EXPECT_EQ(
      kvPairs.str(),
      "units=0.11176&price=357.78&777=-666&hola=quetal&array1=val1,,&array2=,val1,val2,value,&emptyArray=,&price1=42");
  int8_t val = -116;
  kvPairs.set("testu", val);
  EXPECT_EQ(kvPairs.get("testu"), "-116");
}

}  // namespace cct
