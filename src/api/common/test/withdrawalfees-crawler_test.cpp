
#include "withdrawalfees-crawler.hpp"

#include <gtest/gtest.h>

#include "cachedresultvault.hpp"
#include "cct_const.hpp"
#include "coincenterinfo.hpp"
#include "runmodes.hpp"
#include "timedef.hpp"

namespace cct::api {

class WithdrawalFeesCrawlerTest : public ::testing::Test {
 protected:
  settings::RunMode runMode = settings::RunMode::kTestKeys;
  CoincenterInfo coincenterInfo{runMode};
  CachedResultVault cachedResultVault;
  WithdrawalFeesCrawler withdrawalFeesCrawler{coincenterInfo, Duration::max(), cachedResultVault};
};

TEST_F(WithdrawalFeesCrawlerTest, WithdrawalFeesCrawlerService) {
  for (const std::string_view exchangeName : kSupportedExchanges) {
    const auto [amountByCurrencySet, withdrawalMinMap] = withdrawalFeesCrawler.get(exchangeName);

    if (!withdrawalMinMap.empty()) {
      EXPECT_TRUE(true);
      return;
    }
  }
  EXPECT_FALSE(true);
}
}  // namespace cct::api