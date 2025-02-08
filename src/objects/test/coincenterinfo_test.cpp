#include "coincenterinfo.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "cct_string.hpp"
#include "currencycode.hpp"
#include "default-data-dir.hpp"
#include "general-config.hpp"
#include "loadconfiguration.hpp"
#include "monitoringinfo.hpp"
#include "reader_mock.hpp"
#include "runmodes.hpp"

namespace cct {
namespace {
const string kAcronyms(R"json(
    {
        "XBT": "BTC",
        "ZEUR": "EUR"
    })json");

const string kPrefixes(R"json(
    {
        "ARBITRUM": "ARB/",
        "ARBITRO": "ARO/",
        "OPTIMISM": "OPT/"
    })json");
}  // namespace

class CoincenterInfoTest : public ::testing::Test {
 protected:
  LoadConfiguration loadConfiguration{kDefaultDataDir, LoadConfiguration::ExchangeConfigFileType::kTest};
  MockReader currencyAcronymsReader;
  MockReader stableCoinsReader;
  MockReader currencyPrefixesReader;

  CoincenterInfo createCoincenterInfo() const {
    return CoincenterInfo(settings::RunMode::kTestKeysWithProxy, loadConfiguration, schema::GeneralConfig(),
                          LoggingInfo(), MonitoringInfo(), currencyAcronymsReader, stableCoinsReader,
                          currencyPrefixesReader);
  }
};

TEST_F(CoincenterInfoTest, AcronymTestNoData) {
  EXPECT_CALL(currencyAcronymsReader, readAll()).WillOnce(testing::Return(""));
  EXPECT_CALL(stableCoinsReader, readAll()).WillOnce(testing::Return(""));
  EXPECT_CALL(currencyPrefixesReader, readAll()).WillOnce(testing::Return(""));

  auto coincenterInfo = createCoincenterInfo();

  EXPECT_EQ(coincenterInfo.standardizeCurrencyCode("BTC"), CurrencyCode("BTC"));
}

TEST_F(CoincenterInfoTest, AcronymTestNoPrefix) {
  EXPECT_CALL(currencyAcronymsReader, readAll()).WillOnce(testing::Return(kAcronyms));
  EXPECT_CALL(stableCoinsReader, readAll()).WillOnce(testing::Return(""));
  EXPECT_CALL(currencyPrefixesReader, readAll()).WillOnce(testing::Return(""));

  auto coincenterInfo = createCoincenterInfo();

  EXPECT_EQ(coincenterInfo.standardizeCurrencyCode("XBT"), CurrencyCode("BTC"));
  EXPECT_EQ(coincenterInfo.standardizeCurrencyCode("ZJPY"), CurrencyCode("ZJPY"));
}

TEST_F(CoincenterInfoTest, AcronymTestWithPrefix) {
  EXPECT_CALL(currencyAcronymsReader, readAll()).WillOnce(testing::Return(kAcronyms));
  EXPECT_CALL(stableCoinsReader, readAll()).WillOnce(testing::Return(""));
  EXPECT_CALL(currencyPrefixesReader, readAll()).WillOnce(testing::Return(kPrefixes));

  auto coincenterInfo = createCoincenterInfo();

  EXPECT_EQ(coincenterInfo.standardizeCurrencyCode("xbt"), CurrencyCode("BTC"));
  EXPECT_EQ(coincenterInfo.standardizeCurrencyCode("zeur"), CurrencyCode("EUR"));
  EXPECT_EQ(coincenterInfo.standardizeCurrencyCode("ARBITRUM test"), CurrencyCode("ARB/TEST"));
  EXPECT_EQ(coincenterInfo.standardizeCurrencyCode("arbitrum/btc"), CurrencyCode("ARB/BTC"));
  EXPECT_EQ(coincenterInfo.standardizeCurrencyCode("optimismETH"), CurrencyCode("OPT/ETH"));
  EXPECT_EQ(coincenterInfo.standardizeCurrencyCode("ARBItata"), CurrencyCode("ARBITATA"));
}
}  // namespace cct