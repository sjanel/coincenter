#include "flatkeyvaluestring.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string_view>

#include "cct_exception.hpp"
#include "cct_string.hpp"

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

TEST(FlatKeyValueStringTest, SetBackEmpty) {
  KvPairs kvPairs;
  kvPairs.set_back("timestamp", "1621785125200");
  EXPECT_EQ(kvPairs.str(), "timestamp=1621785125200");
}

TEST(FlatKeyValueStringTest, SetAndAppend) {
  KvPairs kvPairs;
  kvPairs.emplace_back("abc", "666");
  kvPairs.push_back({"de", "aX"});
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
  kvPairs.emplace_back("newKey", "=");
  EXPECT_EQ(kvPairs.str(), "abc=777&de=aX&def=titi&777=yoplalepiege&d=cestboncestfini&newKey==");
  kvPairs.emplace_back("$5*(%", ".9h===,Mj");
  EXPECT_EQ(kvPairs.str(), "abc=777&de=aX&def=titi&777=yoplalepiege&d=cestboncestfini&newKey==&$5*(%=.9h===,Mj");
  kvPairs.emplace_back("encoreplustricky", "=");
  EXPECT_EQ(kvPairs.str(),
            "abc=777&de=aX&def=titi&777=yoplalepiege&d=cestboncestfini&newKey==&$5*(%=.9h===,Mj&encoreplustricky==");
  kvPairs.set("$5*(%", ".9h==,Mj");
  EXPECT_EQ(kvPairs.str(),
            "abc=777&de=aX&def=titi&777=yoplalepiege&d=cestboncestfini&newKey==&$5*(%=.9h==,Mj&encoreplustricky==");
}

TEST(FlatKeyValueStringTest, Prepend) {
  KvPairs kvPairs;
  kvPairs.emplace_front("statue", "liberty");
  EXPECT_EQ(kvPairs.str(), "statue=liberty");
  kvPairs.emplace_front("city", "New York City");
  EXPECT_EQ(kvPairs.str(), "city=New York City&statue=liberty");
  kvPairs.push_front({"state", "New York"});
  EXPECT_EQ(kvPairs.str(), "state=New York&city=New York City&statue=liberty");
  kvPairs.emplace_front("Postal Code", 10015);
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

TEST(FlatKeyValueStringTest, SetBack) {
  KvPairs kvPairs{{"abc", "354"}, {"tata", "abc"}, {"rm", "xX"}, {"huhu", "haha"}};
  kvPairs.set_back("abc", "678");
  EXPECT_EQ(kvPairs.str(), "abc=354&tata=abc&rm=xX&huhu=haha&abc=678");
  kvPairs.set_back("abc", "9012");
  EXPECT_EQ(kvPairs.str(), "abc=354&tata=abc&rm=xX&huhu=haha&abc=9012");
}

TEST(FlatKeyValueStringTest, WithNullTerminatingCharAsSeparator) {
  using namespace std::literals;

  using ExoticKeyValuePair = FlatKeyValueString<'\0', ':'>;
  ExoticKeyValuePair kvPairs{{"abc", "354"}, {"tata", "abc"}, {"rm", "xX"}, {"huhu", "haha"}};
  EXPECT_EQ(kvPairs.str(), "abc:354\0tata:abc\0rm:xX\0huhu:haha"sv);
  kvPairs.set("rm", "Yy3");
  EXPECT_EQ(kvPairs.str(), "abc:354\0tata:abc\0rm:Yy3\0huhu:haha"sv);
  kvPairs.erase("abc");
  EXPECT_EQ(kvPairs.str(), "tata:abc\0rm:Yy3\0huhu:haha"sv);
  kvPairs.erase("rm");
  EXPECT_EQ(kvPairs.str(), "tata:abc\0huhu:haha"sv);
  kvPairs.emplace_back("&newField", "&&newValue&&");
  EXPECT_EQ(kvPairs.str(), "tata:abc\0huhu:haha\0&newField:&&newValue&&"sv);

  int kvPairPos = 0;
  for (const auto &kv : kvPairs) {
    const auto key = kv.key();
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
      default:
        throw exception("Too many values in kvPairs");
    }
  }
  EXPECT_EQ(kvPairPos, 3);
}

TEST(FlatKeyValueStringTest, EmptyConvertToJson) { EXPECT_EQ(KvPairs().toJsonStr(), "{}"); }

class FlatKeyValueStringCase1 : public ::testing::Test {
 protected:
  KvPairs kvPairs{{"units", "0.11176"}, {"price", "357.78"},  {"777", "encoredutravail?"},     {"hola", "quetal"},
                  {"k", "v"},           {"array1", "val1,,"}, {"array2", ",val1,val2,value,"}, {"emptyArray", ","}};
};

TEST_F(FlatKeyValueStringCase1, Front) {
  const auto &kvFront = kvPairs.front();

  EXPECT_EQ(kvFront.key(), "units");
  EXPECT_EQ(kvFront.keyLen(), 5U);

  EXPECT_EQ(kvFront.val(), "0.11176");
  EXPECT_EQ(kvFront.valLen(), 7U);

  EXPECT_EQ(kvFront.size(), 13U);
}

TEST_F(FlatKeyValueStringCase1, Back) {
  const auto kvFront = kvPairs.back();

  EXPECT_EQ(kvFront.key(), "emptyArray");
  EXPECT_EQ(kvFront.keyLen(), 10U);

  EXPECT_EQ(kvFront.val(), ",");
  EXPECT_EQ(kvFront.valLen(), 1U);

  EXPECT_EQ(kvFront.size(), 12U);
}

TEST_F(FlatKeyValueStringCase1, PopBack) {
  EXPECT_NE(kvPairs.find("emptyArray"), string::npos);
  kvPairs.pop_back();
  EXPECT_EQ(kvPairs.find("emptyArray"), string::npos);

  const auto newBack = kvPairs.back();
  EXPECT_EQ(newBack.key(), "array2");
  EXPECT_EQ(newBack.val(), ",val1,val2,value,");
}

TEST_F(FlatKeyValueStringCase1, Get) {
  EXPECT_EQ(kvPairs.get("units"), "0.11176");
  EXPECT_EQ(kvPairs.get("price"), "357.78");
  EXPECT_EQ(kvPairs.get("777"), "encoredutravail?");
  EXPECT_EQ(kvPairs.get("hola"), "quetal");
  EXPECT_EQ(kvPairs.get("k"), "v");
  EXPECT_EQ(kvPairs.get("array1"), "val1,,");
  EXPECT_EQ(kvPairs.get("array2"), ",val1,val2,value,");
  EXPECT_EQ(kvPairs.get("emptyArray"), ",");

  EXPECT_EQ(kvPairs.get("laipas"), "");
}

TEST_F(FlatKeyValueStringCase1, ForwardIterator) {
  int itPos = 0;
  for (const auto &kv : kvPairs) {
    const auto key = kv.key();
    const auto val = kv.val();
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
        EXPECT_EQ(key, "k");
        EXPECT_EQ(val, "v");
        break;
      case 5:
        EXPECT_EQ(key, "array1");
        EXPECT_EQ(val, "val1,,");
        break;
      case 6:
        EXPECT_EQ(key, "array2");
        EXPECT_EQ(val, ",val1,val2,value,");
        break;
      case 7:
        EXPECT_EQ(key, "emptyArray");
        EXPECT_EQ(val, ",");
        break;
      default:
        throw exception("Too many values in kvPairs");
    }
  }
  EXPECT_EQ(itPos, 8);
}

TEST_F(FlatKeyValueStringCase1, BackwardIterator) {
  int itPos = 0;
  for (auto it = kvPairs.end(); it != kvPairs.begin();) {
    const auto kv = *--it;
    const auto key = kv.key();
    const auto val = kv.val();
    switch (itPos++) {
      case 0:
        EXPECT_EQ(key, "emptyArray");
        EXPECT_EQ(val, ",");
        break;
      case 1:
        EXPECT_EQ(key, "array2");
        EXPECT_EQ(val, ",val1,val2,value,");
        break;
      case 2:
        EXPECT_EQ(key, "array1");
        EXPECT_EQ(val, "val1,,");
        break;
      case 3:
        EXPECT_EQ(key, "k");
        EXPECT_EQ(val, "v");
        break;
      case 4:
        EXPECT_EQ(key, "hola");
        EXPECT_EQ(val, "quetal");
        break;
      case 5:
        EXPECT_EQ(key, "777");
        EXPECT_EQ(val, "encoredutravail?");
        break;
      case 6:
        EXPECT_EQ(key, "price");
        EXPECT_EQ(val, "357.78");
        break;
      case 7:
        EXPECT_EQ(key, "units");
        EXPECT_EQ(val, "0.11176");
        break;
      default:
        throw exception("Too many values in kvPairs");
    }
  }
  EXPECT_EQ(itPos, 8);
}

TEST_F(FlatKeyValueStringCase1, EraseIncrementDecrement) {
  kvPairs.erase(kvPairs.begin());
  const auto &kvFront = kvPairs.front();

  EXPECT_EQ(kvFront.key(), "price");
  EXPECT_EQ(kvFront.val(), "357.78");

  auto it = kvPairs.begin();
  ++it;
  it++;

  EXPECT_EQ(it->key(), "hola");
  EXPECT_EQ(it->val(), "quetal");

  EXPECT_NE(kvPairs.find("hola"), string::npos);

  kvPairs.erase(it);

  EXPECT_EQ(kvPairs.find("hola"), string::npos);
  EXPECT_NE(kvPairs.find("k"), string::npos);

  it = kvPairs.end()--;
  it--;
  kvPairs.erase(it);

  int itPos = 0;
  for (const auto &kv : kvPairs) {
    const auto key = kv.key();
    const auto val = kv.val();
    switch (itPos++) {
      case 0:
        EXPECT_EQ(key, "price");
        EXPECT_EQ(val, "357.78");
        break;
      case 1:
        EXPECT_EQ(key, "777");
        EXPECT_EQ(val, "encoredutravail?");
        break;
      case 2:
        EXPECT_EQ(key, "k");
        EXPECT_EQ(val, "v");
        break;
      case 3:
        EXPECT_EQ(key, "array1");
        EXPECT_EQ(val, "val1,,");
        break;
      case 4:
        EXPECT_EQ(key, "array2");
        EXPECT_EQ(val, ",val1,val2,value,");
        break;
      default:
        throw exception("Too many values in kvPairs");
    }
  }
  EXPECT_EQ(itPos, 5);
}

TEST_F(FlatKeyValueStringCase1, ConvertToJsonStr) {
  EXPECT_EQ(
      kvPairs.toJsonStr(),
      R"({"units":"0.11176","price":"357.78","777":"encoredutravail?","hola":"quetal","k":"v","array1":["val1",""],"array2":["","val1","val2","value"],"emptyArray":[]})");
}

TEST_F(FlatKeyValueStringCase1, AppendIntegralValues) {
  kvPairs.emplace_back("price1", 1957386078376L);
  EXPECT_EQ(kvPairs.get("price1"), "1957386078376");
  int8_t val = -116;
  kvPairs.emplace_back("testu", val);
  EXPECT_EQ(kvPairs.get("testu"), "-116");
}

TEST_F(FlatKeyValueStringCase1, SetIntegralValues) {
  kvPairs.set("price1", 42);
  EXPECT_EQ(kvPairs.get("price"), "357.78");
  EXPECT_EQ(kvPairs.get("price1"), "42");
  kvPairs.set("777", -666);
  EXPECT_EQ(kvPairs.get("777"), "-666");
  EXPECT_EQ(kvPairs.str(),
            "units=0.11176&price=357.78&777=-666&hola=quetal&k=v&array1=val1,,&array2=,val1,val2,value,&emptyArray=,&"
            "price1=42");
  int8_t val = -116;
  kvPairs.set("testu", val);
  EXPECT_EQ(kvPairs.get("testu"), "-116");
}

TEST_F(FlatKeyValueStringCase1, UrlEncode) {
  EXPECT_EQ(kvPairs.urlEncodeExceptDelimiters().str(),
            "units=0.11176&price=357.78&777=encoredutravail%3F&hola=quetal&k=v&array1=val1%2C%2C&array2=%2Cval1%2Cval2%"
            "2Cvalue%2C&emptyArray=%2C");
}

}  // namespace cct
