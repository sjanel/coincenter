#include "size-bytes-schema.hpp"

#include <gtest/gtest.h>

#include <map>
#include <unordered_map>

#include "cct_json.hpp"
#include "cct_string.hpp"

namespace cct::schema {

using UnorderedMap = std::unordered_map<SizeBytes, bool>;
using Map = std::map<SizeBytes, bool>;

TEST(SizeBytesSchemaTest, FromJsonKey) {
  UnorderedMap map;

  // NOLINTNEXTLINE(readability-implicit-bool-conversion)
  auto ec = json::read<json::opts{.raw_string = true}>(map, R"({"11Ki772":true,"9Mi424Ki200":false})");

  ASSERT_FALSE(ec);

  EXPECT_EQ(map.size(), 2);
  EXPECT_EQ(map.at(SizeBytes{12036}), true);
  EXPECT_EQ(map.at(SizeBytes{9871560}), false);
}

TEST(SizeBytesSchemaTest, ToJsonKey) {
  Map map{{SizeBytes{12036}, true}, {SizeBytes{9871560}, false}};

  string str;

  // NOLINTNEXTLINE(readability-implicit-bool-conversion)
  auto ec = json::write<json::opts{.raw_string = true}>(map, str);

  ASSERT_FALSE(ec);

  EXPECT_EQ(str, R"({"11Ki772":true,"9Mi424Ki200":false})");
}

struct Foo {
  SizeBytes size;
};

TEST(SizeBytesSchemaTest, ToJsonValue) {
  Foo foo;
  foo.size.sizeInBytes = 2415919104;

  string str;

  // NOLINTNEXTLINE(readability-implicit-bool-conversion)
  auto ec = json::write<json::opts{.raw_string = true}>(foo, str);

  ASSERT_FALSE(ec);

  EXPECT_EQ(str, R"({"size":"2Gi256Mi"})");
}

TEST(SizeBytesSchemaTest, FromJsonValue) {
  Foo foo;

  // NOLINTNEXTLINE(readability-implicit-bool-conversion)
  auto ec = json::read<json::opts{.raw_string = true}>(foo, R"({"size":"2Gi256Mi"})");

  ASSERT_FALSE(ec);

  EXPECT_EQ(foo.size.sizeInBytes, 2415919104);
}

}  // namespace cct::schema