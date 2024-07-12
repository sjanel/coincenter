#include "auto-trade-config.hpp"

#include <gtest/gtest.h>

#include <chrono>

#include "cct_json.hpp"
#include "cct_string.hpp"
#include "cct_vector.hpp"

namespace cct::schema {

TEST(AutoTradeConfigTest, StopCriteriaValueParse) {
  EXPECT_EQ(AutoTradeStopCriteriaValue("80%").maxEvolutionPercentage(), 80);
  EXPECT_EQ(AutoTradeStopCriteriaValue("-30%").maxEvolutionPercentage(), -30);
  EXPECT_EQ(AutoTradeStopCriteriaValue("4h").duration(), std::chrono::hours(4));
}

TEST(AutoTradeConfigTest, StopCriteriaRoundTrip) {
  vector<AutoTradeStopCriterion> stopCriteria;

  static constexpr std::string_view kJson =
      R"([{"type":"duration","value":"4h"},{"type":"protectLoss","value":"-30%"},{"type":"secureProfit","value":"80%"}])";

  // NOLINTNEXTLINE(readability-implicit-bool-conversion)
  auto ec = json::read<json::opts_ex{{}, /*raw_string*/ true}>(stopCriteria, kJson);

  ASSERT_FALSE(ec);
  ASSERT_EQ(stopCriteria.size(), 3U);
  EXPECT_EQ(stopCriteria[1].value.maxEvolutionPercentage(), -30);

  string str;

  // NOLINTNEXTLINE(readability-implicit-bool-conversion)
  ec = json::write<json::opts_ex{{}, /*raw_string*/ true}>(stopCriteria, str);

  ASSERT_FALSE(ec);

  EXPECT_EQ(str, kJson);
}

}  // namespace cct::schema
