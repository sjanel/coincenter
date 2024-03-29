
#include "commonapi.hpp"

#include <gtest/gtest.h>

#include "coincenterinfo.hpp"
#include "runmodes.hpp"
#include "timedef.hpp"

namespace cct::api {

class CommonAPITest : public ::testing::Test {
 protected:
  settings::RunMode runMode = settings::RunMode::kTestKeys;
  CoincenterInfo coincenterInfo{runMode};
  CommonAPI commonAPI{coincenterInfo, Duration::max(), Duration::max(), CommonAPI::AtInit::kNoLoadFromFileCache};
};

TEST_F(CommonAPITest, IsFiatService) {
  EXPECT_TRUE(commonAPI.queryIsCurrencyCodeFiat("EUR"));
  EXPECT_TRUE(commonAPI.queryIsCurrencyCodeFiat("KRW"));
  EXPECT_TRUE(commonAPI.queryIsCurrencyCodeFiat("USD"));
  EXPECT_FALSE(commonAPI.queryIsCurrencyCodeFiat("BTC"));
  EXPECT_FALSE(commonAPI.queryIsCurrencyCodeFiat("XRP"));
}

}  // namespace cct::api