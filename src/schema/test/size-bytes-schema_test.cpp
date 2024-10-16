#include "size-bytes-schema.hpp"

#include <gtest/gtest.h>

#include <unordered_map>

#include "cct_json-serialization.hpp"
#include "cct_string.hpp"

namespace cct::schema {

using Map = std::unordered_map<SizeBytes, bool>;

TEST(SizeBytesSchemaTest, FromJsonKey) {
  Map map;

  auto ec = read<opts{.raw_string = true}>(map, R"({"11Ki772":true,"9Mi424Ki200":false})");

  ASSERT_FALSE(ec);

  EXPECT_EQ(map.size(), 2);
  EXPECT_EQ(map.at(SizeBytes{12036}), true);
  EXPECT_EQ(map.at(SizeBytes{9871560}), false);
}

TEST(SizeBytesSchemaTest, ToJsonKey) {
  Map map{{SizeBytes{12036}, true}, {SizeBytes{9871560}, false}};

  string str;

  auto ec = write<opts{.raw_string = true}>(map, str);

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

  auto ec = write<opts{.raw_string = true}>(foo, str);

  ASSERT_FALSE(ec);

  EXPECT_EQ(str, R"({"size":"2Gi256Mi"})");
}

TEST(SizeBytesSchemaTest, FromJsonValue) {
  Foo foo;

  auto ec = read<opts{.raw_string = true}>(foo, R"({"size":"2Gi256Mi"})");

  ASSERT_FALSE(ec);

  EXPECT_EQ(foo.size.sizeInBytes, 2415919104);
}

}  // namespace cct::schema