#include "duration-schema.hpp"

#include <gtest/gtest.h>

#include <unordered_map>

#include "cct_json-serialization.hpp"
#include "cct_string.hpp"

namespace cct::schema {

using Map = std::unordered_map<Duration, bool>;

TEST(DurationSchemaTest, FromJsonKey) {
  Map map;

  auto ec = read<opts{.raw_string = true}>(map, R"({"2w56h":true,"1009s17ms":false})");

  ASSERT_FALSE(ec);

  EXPECT_EQ(map.size(), 2);
  EXPECT_EQ(map.at(Duration{std::chrono::weeks(2) + std::chrono::hours(56)}), true);
  EXPECT_EQ(map.at(Duration{std::chrono::seconds(1009) + std::chrono::milliseconds(17)}), false);
}

TEST(DurationSchemaTest, ToJsonKey) {
  Map map{{Duration{std::chrono::weeks(2) + std::chrono::hours(56)}, true},
          {Duration{std::chrono::seconds(1009) + std::chrono::milliseconds(17)}, false}};

  string str;

  auto ec = write<opts{.raw_string = true}>(map, str);

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

  auto ec = write<opts{.raw_string = true}>(foo, str);

  ASSERT_FALSE(ec);

  EXPECT_EQ(str, R"({"dur":"1mon3d20h13min50s"})");
}

TEST(DurationSchemaTest, FromJsonValue) {
  Foo foo;

  auto ec = read<opts{.raw_string = true}>(foo, R"({"dur":"34d6h42min56s"})");

  ASSERT_FALSE(ec);

  EXPECT_EQ(foo.dur.duration,
            std::chrono::days(34) + std::chrono::hours(6) + std::chrono::minutes(42) + std::chrono::seconds(56));
}

}  // namespace cct::schema