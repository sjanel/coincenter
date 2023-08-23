#include "simpletable.hpp"

#include <gtest/gtest.h>

namespace cct {
TEST(SimpleTable, DefaultConstructor) {
  SimpleTable table;
  EXPECT_TRUE(table.empty());

  std::cout << table;
}

TEST(SimpleTable, OneLinePrint) {
  SimpleTable table;
  string str("I am a string");
  table.emplace_back("Header 1", 42, std::move(str));
  EXPECT_EQ(table.size(), 1U);

  std::cout << table;
}

TEST(SimpleTable, SimplePrint) {
  SimpleTable table;
  SimpleTable::Row row1;
  row1.emplace_back("Amount");
  row1.emplace_back("Currency");
  table.push_back(std::move(row1));
  SimpleTable::Row row2;
  row2.emplace_back("123.45");
  row2.emplace_back("EUR");
  table.push_back(std::move(row2));
  SimpleTable::Row row3;
  row3.emplace_back(65);
  row3.emplace_back("BTC");
  table.push_back(std::move(row3));
  EXPECT_EQ(table.size(), 3U);

  std::cout << table;
}

TEST(SimpleTable, SettingRowDirectly) {
  SimpleTable table("Amount", "Currency", "This header is long and stupid");
  EXPECT_EQ(table.size(), 1U);
  table.emplace_back(1235, "EUR", "Nothing here");
  table.emplace_back("3456.78", "USD", 42);
  table.emplace_back("-677234.67", "SUSHI", -12);
  table.emplace_back(-677256340000, "KEBAB", "-34.09");
  EXPECT_EQ(table[2].front().size(), 7U);
  EXPECT_EQ(table.back().front().size(), 13U);

  std::cout << table;
}
}  // namespace cct