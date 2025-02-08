
#include "withdrawalfees-crawler.hpp"

#include <gtest/gtest.h>

#include "cachedresultvault.hpp"
#include "cct_log.hpp"
#include "coincenterinfo.hpp"
#include "exchange-name-enum.hpp"
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
  for (int exchangeNamePos = 0; exchangeNamePos < kNbSupportedExchanges; ++exchangeNamePos) {
    const auto [amountByCurrencySet, withdrawalMinMap] =
        withdrawalFeesCrawler.get(static_cast<ExchangeNameEnum>(exchangeNamePos));

    if (!withdrawalMinMap.empty()) {
      return;
    }
  }
  log::error("No withdrawal fees data could be retrieved - but do not make test fail as this data is not reliable...");
}
}  // namespace cct::api