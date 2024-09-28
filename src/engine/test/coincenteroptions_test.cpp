#include "coincenteroptions.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <numeric>
#include <sstream>
#include <string_view>
#include <type_traits>
#include <utility>

#include "cct_invalid_argument_exception.hpp"
#include "coincentercommandtype.hpp"
#include "monetaryamount.hpp"
#include "priceoptions.hpp"
#include "priceoptionsdef.hpp"
#include "timedef.hpp"
#include "tradedefinitions.hpp"
#include "tradeoptions.hpp"

namespace cct {

TEST(CoincenterOptionsTest, PrintVersion) {
  std::ostringstream os;
  CoincenterCmdLineOptions::PrintVersion("test", os);
  EXPECT_TRUE(os.view().starts_with("test"));
  EXPECT_NE(os.view().find("curl"), std::string_view::npos);
  EXPECT_NE(os.view().find("OpenSSL"), std::string_view::npos);
}

class CoincenterCmdLineOptionsTest : public ::testing::Test {
 protected:
  CoincenterCmdLineOptions opts;
};

TEST_F(CoincenterCmdLineOptionsTest, DefaultConstructorShouldValueInitializeAll) {
  // This test makes sure that are fields are correctly initialized in the default constructor of
  // CoincenterCmdLineOptions
  alignas(CoincenterCmdLineOptions) std::uint8_t data[sizeof(CoincenterCmdLineOptions)];

  // fill memory with garbage
  std::iota(std::begin(data), std::end(data), static_cast<std::remove_reference_t<decltype(data[0])>>(0));

  // default construct
  ::new (data) CoincenterCmdLineOptions;

  const auto *pRhs = reinterpret_cast<const CoincenterCmdLineOptions *>(data);

  EXPECT_EQ(opts, *pRhs);

  if constexpr (!std::is_trivially_destructible_v<CoincenterCmdLineOptions>) {
    std::destroy_at(pRhs);
  }
}

TEST_F(CoincenterCmdLineOptionsTest, MergeGlobal) {
  opts.async = true;
  opts.balance = "kraken";
  opts.noSecrets = "binance,huobi_user1";

  CoincenterCmdLineOptions rhs;
  rhs.trade = "some value";
  rhs.depth = 42;
  rhs.repeatTime = std::chrono::minutes(45);
  rhs.monitoringPort = 999;

  CoincenterCmdLineOptions expected = opts;

  expected.repeatTime = std::chrono::minutes(45);
  expected.monitoringPort = 999;

  opts.mergeGlobalWith(rhs);

  EXPECT_EQ(expected, opts);
}

TEST_F(CoincenterCmdLineOptionsTest, ComputeTradeOptionsDefault) {
  EXPECT_EQ(opts.computeTradeOptions(), TradeOptions());
}

TEST_F(CoincenterCmdLineOptionsTest, ComputeTradeTypePolicyInvalid) {
  opts.forceMultiTrade = true;
  opts.forceSingleTrade = true;
  EXPECT_THROW(opts.computeTradeOptions(), invalid_argument);
}

TEST_F(CoincenterCmdLineOptionsTest, ComputeTradeTimeoutActionInvalid) {
  opts.tradeTimeoutCancel = true;
  opts.tradeTimeoutMatch = true;
  EXPECT_THROW(opts.computeTradeOptions(), invalid_argument);
}

TEST_F(CoincenterCmdLineOptionsTest, ComputeTradeOptionsTradeStrategy) {
  opts.tradeStrategy = "nibble";
  opts.tradeTimeoutMatch = true;
  opts.isSimulation = true;
  EXPECT_EQ(opts.computeTradeOptions(), TradeOptions(PriceOptions(opts.tradeStrategy), TradeTimeoutAction::kMatch,
                                                     TradeMode::kSimulation, opts.tradeTimeout, opts.tradeUpdatePrice,
                                                     TradeTypePolicy::kDefault, TradeSyncPolicy::kSynchronous));
}

TEST_F(CoincenterCmdLineOptionsTest, ComputeTradeOptionsTradeInvalidTradePrice) {
  opts.tradePrice = "4.5";
  opts.sellAll = "USDT";
  EXPECT_THROW(opts.computeTradeOptions(), invalid_argument);
}

TEST_F(CoincenterCmdLineOptionsTest, ComputeTradeOptionsTradePriceNeutral) {
  opts.tradePrice = "4";
  EXPECT_EQ(opts.computeTradeOptions(), TradeOptions(PriceOptions(RelativePrice(MonetaryAmount(4).integerPart()))));
}

TEST_F(CoincenterCmdLineOptionsTest, ComputeTradeOptionsTradePrice) {
  opts.tradePrice = "4XRP";
  opts.tradeTimeout = seconds(100);
  opts.async = true;
  EXPECT_EQ(
      opts.computeTradeOptions(),
      TradeOptions(PriceOptions(MonetaryAmount(4, "XRP")), TradeTimeoutAction::kDefault, TradeMode::kReal, seconds(100),
                   opts.tradeUpdatePrice, TradeTypePolicy::kForceSingleTrade, TradeSyncPolicy::kAsynchronous));
}

TEST_F(CoincenterCmdLineOptionsTest, ComputeTradeArgStrDefault) { EXPECT_TRUE(opts.getTradeArgStr().first.empty()); }

TEST_F(CoincenterCmdLineOptionsTest, ComputeTradeArgStrInvalid) {
  opts.tradeStrategy = "nibble";
  opts.tradePrice = "2";
  EXPECT_THROW(opts.getTradeArgStr(), invalid_argument);
}

TEST_F(CoincenterCmdLineOptionsTest, ComputeTradeArgStrTrade) {
  opts.trade = "some value";
  EXPECT_EQ(opts.getTradeArgStr(), std::make_pair(opts.trade, CoincenterCommandType::kTrade));
}

TEST_F(CoincenterCmdLineOptionsTest, ComputeTradeArgStrTradeAll) {
  opts.tradeAll = "some value";
  EXPECT_EQ(opts.getTradeArgStr(), std::make_pair(opts.tradeAll, CoincenterCommandType::kTrade));
}

TEST_F(CoincenterCmdLineOptionsTest, ComputeTradeArgStrSellAll) {
  opts.sellAll = "some value";
  EXPECT_EQ(opts.getTradeArgStr(), std::make_pair(opts.sellAll, CoincenterCommandType::kSell));
}

TEST_F(CoincenterCmdLineOptionsTest, ComputeTradeArgStrSell) {
  opts.sell = "some value";
  EXPECT_EQ(opts.getTradeArgStr(), std::make_pair(opts.sell, CoincenterCommandType::kSell));
}

TEST_F(CoincenterCmdLineOptionsTest, ComputeTradeArgStrBuy) {
  opts.buy = "some value";
  EXPECT_EQ(opts.getTradeArgStr(), std::make_pair(opts.buy, CoincenterCommandType::kBuy));
}
}  // namespace cct
