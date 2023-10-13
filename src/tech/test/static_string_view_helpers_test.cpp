#include "static_string_view_helpers.hpp"

#include <string_view>

namespace cct {
// JoinStringView
namespace test1 {
static_assert(JoinStringView_v<>.empty());
}  // namespace test1

namespace test2 {
static constexpr std::string_view kStr1 = "hello";

static_assert(JoinStringView_v<kStr1> == "hello");
}  // namespace test2

namespace test3 {
static constexpr std::string_view kStr1 = "this";
static constexpr std::string_view kStr2 = " is a ";
static constexpr std::string_view kStr3 = "composed ";
static constexpr std::string_view kStr4 = "string";

static_assert(JoinStringView_v<kStr1, kStr2, kStr3, kStr4> == "this is a composed string");
}  // namespace test3

namespace test4 {
static constexpr std::string_view kStr1 = "The letter coming after ";
static constexpr std::string_view kStr2 = " is ";

static_assert(JoinStringView_v<kStr1, CharToStringView_v<'g'>, kStr2, CharToStringView_v<'h'>> ==
              "The letter coming after g is h");
}  // namespace test4

namespace test5 {
static constexpr std::string_view kStr1 = "I have ";
static constexpr std::string_view kStr2 = " oranges and ";
static constexpr std::string_view kStr3 = " bananas ";
static constexpr std::string_view kStr4 = "in my bag";

static_assert(JoinStringView_v<kStr1, IntToStringView_v<70>, kStr2, IntToStringView_v<1894>, kStr3, kStr4> ==
              "I have 70 oranges and 1894 bananas in my bag");
}  // namespace test5

// IntToStringView
static_assert(IntToStringView_v<0> == "0");
static_assert(IntToStringView_v<37> == "37");
static_assert(IntToStringView_v<-1273006> == "-1273006");
}  // namespace cct