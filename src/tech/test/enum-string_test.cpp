#include "enum-string.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <glaze/glaze.hpp>  // IWYU pragma: export

#include "cct_invalid_argument_exception.hpp"

namespace cct {

enum class TestEnum : int8_t { first, second, third, fourth, a, b, c };

}

template <>
struct glz::meta<::cct::TestEnum> {
  using enum ::cct::TestEnum;

  static constexpr auto value = enumerate(first, second, third, fourth, a, b, c);
};

namespace cct {

TEST(EnumStringTest, EnumToString) {
  EXPECT_EQ(EnumToString(TestEnum::first), "first");
  EXPECT_EQ(EnumToString(TestEnum::second), "second");
  EXPECT_EQ(EnumToString(TestEnum::third), "third");
  EXPECT_EQ(EnumToString(TestEnum::fourth), "fourth");
  EXPECT_EQ(EnumToString(TestEnum::a), "a");
  EXPECT_EQ(EnumToString(TestEnum::b), "b");
  EXPECT_EQ(EnumToString(TestEnum::c), "c");

  static_assert(EnumToString(TestEnum::first) == "first");
  static_assert(EnumToString(TestEnum::second) == "second");
}

TEST(EnumStringTest, EnumFromString) {
  EXPECT_EQ(EnumFromString<TestEnum>("first"), TestEnum::first);
  EXPECT_EQ(EnumFromString<TestEnum>("second"), TestEnum::second);
  EXPECT_EQ(EnumFromString<TestEnum>("third"), TestEnum::third);
  EXPECT_EQ(EnumFromString<TestEnum>("fourth"), TestEnum::fourth);
  EXPECT_EQ(EnumFromString<TestEnum>("a"), TestEnum::a);
  EXPECT_EQ(EnumFromString<TestEnum>("b"), TestEnum::b);
  EXPECT_EQ(EnumFromString<TestEnum>("c"), TestEnum::c);

  EXPECT_THROW(EnumFromString<TestEnum>("bad"), invalid_argument);
  EXPECT_THROW(EnumFromString<TestEnum>("firsU"), invalid_argument);
  EXPECT_THROW(EnumFromString<TestEnum>("fifth"), invalid_argument);
  EXPECT_THROW(EnumFromString<TestEnum>("A"), invalid_argument);

  static_assert(EnumFromString<TestEnum>("first") == TestEnum::first);
  static_assert(EnumFromString<TestEnum>("second") == TestEnum::second);
  static_assert(EnumFromString<TestEnum>("third") == TestEnum::third);
}

TEST(EnumStringTest, EnumFromStringCaseInsensitive) {
  EXPECT_EQ(EnumFromStringCaseInsensitive<TestEnum>("fiRsT"), TestEnum::first);
  EXPECT_EQ(EnumFromStringCaseInsensitive<TestEnum>("seCONd"), TestEnum::second);
  EXPECT_EQ(EnumFromStringCaseInsensitive<TestEnum>("Third"), TestEnum::third);
  EXPECT_EQ(EnumFromStringCaseInsensitive<TestEnum>("FOURTH"), TestEnum::fourth);
  EXPECT_EQ(EnumFromStringCaseInsensitive<TestEnum>("a"), TestEnum::a);
  EXPECT_EQ(EnumFromStringCaseInsensitive<TestEnum>("B"), TestEnum::b);
  EXPECT_EQ(EnumFromStringCaseInsensitive<TestEnum>("C"), TestEnum::c);

  EXPECT_THROW(EnumFromStringCaseInsensitive<TestEnum>("bad"), invalid_argument);
  EXPECT_THROW(EnumFromStringCaseInsensitive<TestEnum>("firsU"), invalid_argument);
  EXPECT_THROW(EnumFromStringCaseInsensitive<TestEnum>("fifth"), invalid_argument);
  EXPECT_THROW(EnumFromStringCaseInsensitive<TestEnum>("d"), invalid_argument);

  static_assert(EnumFromStringCaseInsensitive<TestEnum>("fiRsT") == TestEnum::first);
  static_assert(EnumFromStringCaseInsensitive<TestEnum>("seCONd") == TestEnum::second);
  static_assert(EnumFromStringCaseInsensitive<TestEnum>("Third") == TestEnum::third);
}

}  // namespace cct