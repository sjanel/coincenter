#pragma once

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <sstream>
#include <string_view>

#include "apioutputtype.hpp"
#include "cct_json.hpp"
#include "exchangedata_test.hpp"
#include "queryresultprinter.hpp"
#include "timedef.hpp"

namespace cct {

class QueryResultPrinterTest : public ExchangesBaseTest {
 protected:
  TimePoint tp1{milliseconds{std::numeric_limits<int64_t>::max() / 10000000}};
  TimePoint tp2{milliseconds{std::numeric_limits<int64_t>::max() / 9000000}};
  TimePoint tp3{milliseconds{std::numeric_limits<int64_t>::max() / 8000000}};
  TimePoint tp4{milliseconds{std::numeric_limits<int64_t>::max() / 7000000}};

  TradeOptions defaultTradeOptions{TradeOptions{},
                                   coincenterInfo.exchangeConfig(exchangePublic1.exchangeNameEnum()).query.trade};

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

    glz::json_t lhs;
    glz::json_t rhs;

    ASSERT_FALSE(glz::read_json(lhs, ss.view()));
    ASSERT_FALSE(glz::read_json(rhs, expected));

    EXPECT_EQ(lhs.dump(), rhs.dump());
  }

  QueryResultPrinter basicQueryResultPrinter(ApiOutputType apiOutputType) {
    return {ss, apiOutputType, coincenterInfo.loggingInfo()};
  }

  std::ostringstream ss;
};
}  // namespace cct