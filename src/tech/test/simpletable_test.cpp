#include "simpletable.hpp"

#include <gtest/gtest.h>

namespace cct {
TEST(SimpleTable, DefaultConstructor) {
  SimpleTable t;
  EXPECT_TRUE(t.empty());
  t.print(std::cout);
}

TEST(SimpleTable, OneLinePrint) {
  SimpleTable t;
  string s("I am a string");
  t.emplace_back("Header 1", 42, std::move(s));
  EXPECT_EQ(t.size(), 1U);
  t.print(std::cout);
}

TEST(SimpleTable, SimplePrint) {
  SimpleTable t;
  SimpleTable::Row row1;
  row1.emplace_back("Amount");
  row1.emplace_back("Currency");
  t.push_back(std::move(row1));
  SimpleTable::Row row2;
  row2.emplace_back("123.45");
  row2.emplace_back("EUR");
  t.push_back(std::move(row2));
  SimpleTable::Row row3;
  row3.emplace_back(65);
  row3.emplace_back("BTC");
  t.push_back(std::move(row3));
  EXPECT_EQ(t.size(), 3U);
  t.print(std::cout);
}

TEST(SimpleTable, SettingRowDirectly) {
  SimpleTable t("Amount", "Currency", "This header is long and stupid");
  EXPECT_EQ(t.size(), 1U);
  t.emplace_back(1235, "EUR", "Nothing here");
  t.emplace_back("3456.78", "USD", 42);
  t.emplace_back("-677234.67", "SUSHI", -12);
  t.emplace_back(-67725634, "KEBAB", "-34.09");
  t.print(std::cout);
}
}  // namespace cct