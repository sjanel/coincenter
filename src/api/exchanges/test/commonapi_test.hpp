#pragma once

#include <gtest/gtest.h>

#include "apikeysprovider.hpp"
#include "coincenterinfo.hpp"
#include "cryptowatchapi.hpp"
#include "fiatconverter.hpp"

namespace cct {

ExchangeInfoMap ComputeExchangeInfoMap(std::string_view) {
  ExchangeInfoMap ret;
  for (std::string_view exchangeName : kSupportedExchanges) {
    ret.insert_or_assign(string(exchangeName), ExchangeInfo(exchangeName, "0.1", "0.1", std::span<const CurrencyCode>(),
                                                            std::span<const CurrencyCode>(), 1000, 1000, false, false));
  }
  return ret;
}

namespace api {

template <class PublicExchangeT>
class TestAPI : public ::testing::Test {
 protected:
  TestAPI()
      : coincenterInfo(settings::RunMode::kProd, kDefaultDataDir),
        coincenterTestInfo(settings::RunMode::kTest),
        apiKeyProvider(coincenterInfo.dataDir(), coincenterInfo.getRunMode()),
        apiTestKeyProvider(coincenterTestInfo.dataDir(), coincenterTestInfo.getRunMode()),
        fiatConverter(coincenterInfo),
        cryptowatchAPI(coincenterInfo),
        exchangePublic(coincenterInfo, fiatConverter, cryptowatchAPI) {}

  virtual void SetUp() {}
  virtual void TearDown() {}

  CoincenterInfo coincenterInfo;
  CoincenterInfo coincenterTestInfo;
  APIKeysProvider apiKeyProvider;
  APIKeysProvider apiTestKeyProvider;
  FiatConverter fiatConverter;
  CryptowatchAPI cryptowatchAPI;
  PublicExchangeT exchangePublic;
};
}  // namespace api
}  // namespace cct