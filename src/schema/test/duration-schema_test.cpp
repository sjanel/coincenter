#include "duration-schema.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <map>
#include <unordered_map>

#include "cct_json.hpp"
#include "cct_string.hpp"

namespace cct::schema {

using UnorderedMap = std::unordered_map<Duration, bool>;
using Map = std::map<Duration, bool>;

TEST(DurationSchemaTest, FromJsonKey) {
  UnorderedMap map;

  // NOLINTNEXTLINE(readability-implicit-bool-conversion)
  auto ec = json::read<json::opts{.raw_string = true}>(map, R"({"2w56h":true,"1009s17ms":false})");

  ASSERT_FALSE(ec);

  EXPECT_EQ(map.size(), 2);
  EXPECT_EQ(map.at(Duration{std::chrono::weeks(2) + std::chrono::hours(56)}), true);
  EXPECT_EQ(map.at(Duration{std::chrono::seconds(1009) + std::chrono::milliseconds(17)}), false);
}

TEST(DurationSchemaTest, ToJsonKey) {
  Map map{{Duration{std::chrono::weeks(2) + std::chrono::hours(56)}, true},
          {Duration{std::chrono::seconds(1009) + std::chrono::milliseconds(17)}, false}};

  string str;

  // NOLINTNEXTLINE(readability-implicit-bool-conversion)
  auto ec = json::write<json::opts{.raw_string = true}>(map, str);

  ASSERT_FALSE(ec);

  EXPECT_EQ(str, R"({"16min49s17ms":false,"2w2d8h":true})");
}

struct Foo {
  Duration dur;
};

TEST(DurationSchemaTest, ToJsonValue) {
  Foo foo;
  foo.dur.duration =
      std::chrono::days(34) + std::chrono::hours(6) + std::chrono::minutes(42) + std::chrono::seconds(56);

  string str;

  // NOLINTNEXTLINE(readability-implicit-bool-conversion)
  auto ec = json::write<json::opts{.raw_string = true}>(foo, str);

  ASSERT_FALSE(ec);

  EXPECT_EQ(str, R"({"dur":"1mon3d20h13min50s"})");
}

TEST(DurationSchemaTest, FromJsonValue) {
  Foo foo;

  // NOLINTNEXTLINE(readability-implicit-bool-conversion)
  auto ec = json::read<json::opts{.raw_string = true}>(foo, R"({"dur":"34d6h42min56s"})");

  ASSERT_FALSE(ec);

  EXPECT_EQ(foo.dur.duration,
            std::chrono::days(34) + std::chrono::hours(6) + std::chrono::minutes(42) + std::chrono::seconds(56));
}

}  // namespace cct::schema