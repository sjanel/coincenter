
#include "withdrawalfees-crawler.hpp"

#include <gtest/gtest.h>

#include <array>
#include <string_view>

#include "cachedresultvault.hpp"
#include "cct_log.hpp"
#include "coincenterinfo.hpp"
#include "exchange-name-enum.hpp"
#include "runmodes.hpp"
#include "timedef.hpp"

namespace cct::api {

class WithdrawalFeesCrawlerTest : public ::testing::Test {
 protected:
  static WithdrawalFeesCrawler::WithdrawalInfoMaps ParseBithumbResponse(std::string_view dataStr) {
    return WithdrawalFeesCrawler::ParseBithumbResponse(dataStr);
  }

  static WithdrawalFeesCrawler::WithdrawalInfoMaps ParseKrakenResponse(std::string_view dataStr) {
    return WithdrawalFeesCrawler::ParseKrakenResponse(dataStr);
  }

  settings::RunMode runMode = settings::RunMode::kTestKeys;
  CoincenterInfo coincenterInfo{runMode};
  CachedResultVault cachedResultVault;
  WithdrawalFeesCrawler withdrawalFeesCrawler{coincenterInfo, Duration::max(), cachedResultVault};
};

TEST_F(WithdrawalFeesCrawlerTest, ParseBithumbResponse) {
  constexpr std::string_view data = R"json([
    {"currency":"BTC","networks":[
      {"withdraw_fee_quantity":"0.0002","withdraw_minimum_quantity":"0.001"}
    ]},
    {"currency":"ETH","networks":[
      {"withdraw_fee_quantity":"0.005","withdraw_minimum_quantity":"0.01"},
      {"withdraw_fee_quantity":"0.00000175","withdraw_minimum_quantity":"0.1"}
    ]},
    {"currency":"RATE","networks":[
      {"withdraw_fee_quantity":null,"withdraw_minimum_quantity":"2","withdraw_rate":"0.01"}
    ]}
  ])json";

  const auto [fees, minimums] = ParseBithumbResponse(data);

  ASSERT_EQ(fees.size(), 2);
  EXPECT_EQ(fees.getOrThrow("BTC"), MonetaryAmount("0.0002 BTC"));
  EXPECT_EQ(fees.getOrThrow("ETH"), MonetaryAmount("0.005 ETH"));
  EXPECT_EQ(minimums.at("BTC"), MonetaryAmount("0.001 BTC"));
  EXPECT_EQ(minimums.at("ETH"), MonetaryAmount("0.01 ETH"));
  EXPECT_FALSE(fees.contains("RATE"));
}

TEST_F(WithdrawalFeesCrawlerTest, ParseKrakenResponse) {
  constexpr std::string_view data = R"json({"result":[
    {"asset":"BTC","fee":"0.00001","min_amount":"0.0001",
     "withdrawal_network_info":{"network":"Lightning"}},
    {"asset":"BTC","fee":"0.0002","min_amount":"0.001",
     "withdrawal_network_info":{"network":"Bitcoin"}},
    {"asset":"ETH","fee":"0.003","min_amount":"0.01",
     "withdrawal_network_info":{"network":"Ethereum"}}
  ],"errors":[]})json";

  const auto [fees, minimums] = ParseKrakenResponse(data);

  ASSERT_EQ(fees.size(), 2);
  EXPECT_EQ(fees.getOrThrow("BTC"), MonetaryAmount("0.0002 BTC"));
  EXPECT_EQ(fees.getOrThrow("ETH"), MonetaryAmount("0.003 ETH"));
  EXPECT_EQ(minimums.at("BTC"), MonetaryAmount("0.001 BTC"));
  EXPECT_EQ(minimums.at("ETH"), MonetaryAmount("0.01 ETH"));
}

TEST_F(WithdrawalFeesCrawlerTest, WithdrawalFeesCrawlerService) {
  static constexpr std::array kCrawlerExchanges{ExchangeNameEnum::bithumb, ExchangeNameEnum::kraken};
  bool foundWithdrawalFees = false;
  for (const ExchangeNameEnum exchangeName : kCrawlerExchanges) {
    const auto& [fees, minimums] = withdrawalFeesCrawler.get(exchangeName);
    foundWithdrawalFees |= !fees.empty() && !minimums.empty();
  }

  if (!foundWithdrawalFees) {
    log::error(
        "No withdrawal fees data could be retrieved - but do not make test fail as external data may be "
        "temporarily unavailable...");
  }
}
}  // namespace cct::api
