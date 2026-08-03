#pragma once

#include <gtest/gtest.h>

#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <string_view>

#include "apioutputtype.hpp"
#include "cct_json.hpp"
#include "exchangedata_test.hpp"
#include "queryresultprinter.hpp"
#include "timedef.hpp"

namespace cct {

// Order-insensitive JSON document used to compare printer output: glz::json_t (ordered_small_map)
// preserves object key insertion order, so we parse into a sorted-map based generic_json instead to
// normalize key ordering before comparing (the printer output order is not part of the contract).
template <class V>
using SortedJsonMap = std::map<std::string, V, std::less<>>;
using OrderInsensitiveJson = glz::generic_json<glz::num_mode::f64, SortedJsonMap>;

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

    OrderInsensitiveJson lhs;
    OrderInsensitiveJson rhs;

    ASSERT_FALSE(glz::read_json(lhs, ss.view()));
    ASSERT_FALSE(glz::read_json(rhs, expected));

    const auto lhsDump = lhs.dump();
    const auto rhsDump = rhs.dump();
    ASSERT_TRUE(lhsDump.has_value());
    ASSERT_TRUE(rhsDump.has_value());
    EXPECT_EQ(lhsDump.value(), rhsDump.value());
  }

  QueryResultPrinter basicQueryResultPrinter(ApiOutputType apiOutputType) {
    return {ss, apiOutputType, coincenterInfo.loggingInfo()};
  }

  std::ostringstream ss;
};
}  // namespace cct