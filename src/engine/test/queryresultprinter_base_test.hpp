#include <gtest/gtest.h>

#include <limits>
#include <sstream>
#include <string_view>

#include "cct_config.hpp"
#include "exchangedata_test.hpp"
#include "timedef.hpp"

namespace cct {

class QueryResultPrinterTest : public ExchangesBaseTest {
 protected:
  TimePoint tp1{TimeInMs{std::numeric_limits<int64_t>::max() / 10000000}};
  TimePoint tp2{TimeInMs{std::numeric_limits<int64_t>::max() / 9000000}};
  TimePoint tp3{TimeInMs{std::numeric_limits<int64_t>::max() / 8000000}};
  TimePoint tp4{TimeInMs{std::numeric_limits<int64_t>::max() / 7000000}};

  void SetUp() override { ss.clear(); }

  void expectNoStr() const { EXPECT_TRUE(ss.view().empty()); }

  void expectStr(std::string_view expected) const {
    ASSERT_FALSE(expected.empty());
    expected.remove_prefix(1);  // skip first newline char of expected string
    EXPECT_EQ(ss.view(), expected);
  }

  void expectJson(std::string_view expected) const {
    ASSERT_FALSE(expected.empty());
    expected.remove_prefix(1);  // skip first newline char of expected string
    EXPECT_EQ(json::parse(ss.view()), json::parse(expected));
  }

  QueryResultPrinter basicQueryResultPrinter(ApiOutputType apiOutputType) {
    return QueryResultPrinter(ss, apiOutputType, coincenterInfo.loggingInfo());
  }

  std::ostringstream ss;
};
}  // namespace cct