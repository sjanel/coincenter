#include "simpletable.hpp"

#include <gtest/gtest.h>

#include <sstream>
#include <string_view>
#include <utility>

#include "cct_string.hpp"

namespace cct {
TEST(SimpleTable, DefaultConstructor) {
  SimpleTable table;
  EXPECT_TRUE(table.empty());

  std::ostringstream ss;
  ss << table;
  EXPECT_TRUE(ss.view().empty());
}

TEST(SimpleTable, OneLinePrint) {
  SimpleTable table;
  string str("I am a string");
  table.emplace_back("Header 1", 42, std::move(str));
  EXPECT_EQ(table.size(), 1U);

  std::ostringstream ss;

  ss << '\n' << table;
  static constexpr std::string_view kExpected = R"(
+----------+----+---------------+
| Header 1 | 42 | I am a string |
+----------+----+---------------+)";

  EXPECT_EQ(ss.view(), kExpected);
}

TEST(SimpleTable, SimplePrint) {
  SimpleTable table;
  table::Row row1;
  row1.emplace_back("Amount");
  row1.emplace_back("Currency");
  row1.emplace_back("Is Fiat");
  table.push_back(std::move(row1));
  table::Row row2;
  row2.emplace_back("123.45");
  row2.emplace_back("EUR");
  row2.emplace_back(true);
  table.push_back(std::move(row2));
  table::Row row3;
  row3.emplace_back(65);
  row3.emplace_back("BTC");
  row3.emplace_back(false);
  table.push_back(std::move(row3));
  EXPECT_EQ(table.size(), 3U);

  std::ostringstream ss;

  ss << '\n' << table;
  static constexpr std::string_view kExpected = R"(
+--------+----------+---------+
| Amount | Currency | Is Fiat |
+--------+----------+---------+
| 123.45 | EUR      | yes     |
| 65     | BTC      | no      |
+--------+----------+---------+)";

  EXPECT_EQ(ss.view(), kExpected);
}

class SimpleTableTest : public ::testing::Test {
 protected:
  void fill() {
    table.emplace_back(1235, "EUR", "Nothing here");
    table.emplace_back("3456.78", "USD", 42);
    table.emplace_back("-677234.67", "SUSHI", -12);
    table.emplace_back(-677256340000, "KEBAB", "-34.09");
  }

  SimpleTable table{table::Row("Amount", "Currency", "This header is longer")};
};

TEST_F(SimpleTableTest, SettingRowDirectly) {
  EXPECT_EQ(table.size(), 1U);
  fill();
  EXPECT_EQ(table[2].front().size(), 1U);
  EXPECT_EQ(table[2].front().front().width(), 7U);

  EXPECT_EQ(table.back().front().size(), 1U);
  EXPECT_EQ(table.back().front().front().width(), 13U);

  std::ostringstream ss;

  ss << '\n' << table;
  static constexpr std::string_view kExpected = R"(
+---------------+----------+-----------------------+
| Amount        | Currency | This header is longer |
+---------------+----------+-----------------------+
| 1235          | EUR      | Nothing here          |
| 3456.78       | USD      | 42                    |
| -677234.67    | SUSHI    | -12                   |
| -677256340000 | KEBAB    | -34.09                |
+---------------+----------+-----------------------+)";

  EXPECT_EQ(ss.view(), kExpected);
}

TEST_F(SimpleTableTest, MultiLineFields) {
  fill();

  table[1][2].push_back(table::CellLine("... but another line!"));
  table[3][0].push_back(table::CellLine(true));

  table.emplace_back("999.25", "KRW", 16820100000000000000UL);

  std::ostringstream ss;

  ss << '\n' << table;

  static constexpr std::string_view kExpected = R"(
+---------------+----------+-----------------------+
| Amount        | Currency | This header is longer |
+---------------+----------+-----------------------+
| 1235          | EUR      | Nothing here          |
|               |          | ... but another line! |
|~~~~~~~~~~~~~~~|~~~~~~~~~~|~~~~~~~~~~~~~~~~~~~~~~~|
| 3456.78       | USD      | 42                    |
|~~~~~~~~~~~~~~~|~~~~~~~~~~|~~~~~~~~~~~~~~~~~~~~~~~|
| -677234.67    | SUSHI    | -12                   |
| yes           |          |                       |
|~~~~~~~~~~~~~~~|~~~~~~~~~~|~~~~~~~~~~~~~~~~~~~~~~~|
| -677256340000 | KEBAB    | -34.09                |
| 999.25        | KRW      | 16820100000000000000  |
+---------------+----------+-----------------------+)";

  EXPECT_EQ(ss.view(), kExpected);
}

TEST_F(SimpleTableTest, EmptyCellShouldBePossible) {
  fill();

  table.emplace_back(table::Cell{12, -4}, table::Cell{}, "Nothing here");

  std::ostringstream ss;

  ss << '\n' << table;

  static constexpr std::string_view kExpected = R"(
+---------------+----------+-----------------------+
| Amount        | Currency | This header is longer |
+---------------+----------+-----------------------+
| 1235          | EUR      | Nothing here          |
| 3456.78       | USD      | 42                    |
| -677234.67    | SUSHI    | -12                   |
| -677256340000 | KEBAB    | -34.09                |
|~~~~~~~~~~~~~~~|~~~~~~~~~~|~~~~~~~~~~~~~~~~~~~~~~~|
| 12            |          | Nothing here          |
| -4            |          |                       |
+---------------+----------+-----------------------+)";
  EXPECT_EQ(ss.view(), kExpected);
}

class DividerLineTest : public ::testing::Test {
 protected:
  void SetUp() override { fill(); }

  void fill() {
    table.emplace_back(1);
    table.emplace_back(2);
    table.emplace_back("");
    table.emplace_back();
    table.emplace_back(4);
    table.emplace_back();
  }

  SimpleTable table;
  std::ostringstream ss;
};

TEST_F(DividerLineTest, SingleLineRows) {
  ss << '\n' << table;

  static constexpr std::string_view kExpected = R"(
+---+
| 1 |
+---+
| 2 |
|   |
+---+
| 4 |
+---+
+---+)";

  EXPECT_EQ(ss.view(), kExpected);
}

TEST_F(DividerLineTest, WithMultiLine) {
  table[1][0].push_back(table::CellLine(42));
  table[1][0].push_back(table::CellLine(true));

  table[4][0].push_back(table::CellLine(false));

  ss << '\n' << table;

  static constexpr std::string_view kExpected = R"(
+-----+
| 1   |
+-----+
| 2   |
| 42  |
| yes |
|~~~~~|
|     |
+-----+
| 4   |
| no  |
+-----+
+-----+)";

  EXPECT_EQ(ss.view(), kExpected);
}

}  // namespace cct