#include "binance-common-api.hpp"

#include <algorithm>
#include <mutex>
#include <string_view>
#include <utility>

#include "abstractmetricgateway.hpp"
#include "binance-common-schema.hpp"
#include "cachedresult.hpp"
#include "cct_log.hpp"
#include "cct_smallvector.hpp"
#include "curlhandle.hpp"
#include "currencycode.hpp"
#include "currencycodeset.hpp"
#include "currencyexchange.hpp"
#include "currencyexchangeflatset.hpp"
#include "httprequesttype.hpp"
#include "monetary-amount-vector.hpp"
#include "monetaryamount.hpp"
#include "monetaryamountbycurrencyset.hpp"
#include "permanentcurloptions.hpp"
#include "request-retry.hpp"
#include "runmodes.hpp"

namespace cct::api {

namespace {

constexpr std::string_view kCryptoFeeBaseUrl = "https://www.binance.com";
}  // namespace

BinanceGlobalInfos::BinanceGlobalInfosFunc::BinanceGlobalInfosFunc(AbstractMetricGateway* pMetricGateway,
                                                                   const PermanentCurlOptions& permanentCurlOptions,
                                                                   settings::RunMode runMode)
    : _curlHandle(kCryptoFeeBaseUrl, pMetricGateway, permanentCurlOptions, runMode) {}

schema::binance::NetworkCoinDataVector BinanceGlobalInfos::BinanceGlobalInfosFunc::operator()() {
  RequestRetry requestRetry(_curlHandle, CurlOptions(HttpRequestType::kGet));

  auto ret = requestRetry.query<schema::binance::NetworkCoinAll>(
      "/bapi/capital/v1/public/capital/getNetworkCoinAll", [](const auto& response) {
        static constexpr std::string_view kExpectedCode = "000000";
        if (response.code != kExpectedCode) {
          log::warn("Binance error ({})", response.code);
          return RequestRetry::Status::kResponseError;
        }
        return RequestRetry::Status::kResponseOK;
      });

  const auto [endIt, oldEndIt] =
      std::ranges::remove_if(ret.data, [](const auto& el) { return el.coin.size() > CurrencyCode::kMaxLen; });

  if (endIt != ret.data.end()) {
    log::debug("{} currencies discarded for binance as code too long", ret.data.end() - endIt);
    ret.data.erase(endIt, ret.data.end());
  }

  std::ranges::sort(ret.data);

  return ret.data;
}

namespace {
MonetaryAmount ComputeWithdrawalFeesFromNetworkList(CurrencyCode cur, const auto& coinElem) {
  MonetaryAmount withdrawFee(0, cur);
  for (const auto& networkListPart : coinElem.networkList) {
    MonetaryAmount fee(networkListPart.withdrawFee, cur);
    if (networkListPart.isDefault) {
      withdrawFee = fee;
      break;
    }
    withdrawFee = std::max(withdrawFee, fee);
  }
  return withdrawFee;
}
}  // namespace

BinanceGlobalInfos::BinanceGlobalInfos(CachedResultOptions&& cachedResultOptions, AbstractMetricGateway* pMetricGateway,
                                       const PermanentCurlOptions& permanentCurlOptions, settings::RunMode runMode)
    : _globalInfosCache(cachedResultOptions, pMetricGateway, permanentCurlOptions, runMode) {}

MonetaryAmountByCurrencySet BinanceGlobalInfos::queryWithdrawalFees() {
  std::lock_guard<std::mutex> guard(_mutex);
  const auto& allCoins = _globalInfosCache.get();

  MonetaryAmountVector fees(allCoins.size());

  std::ranges::transform(allCoins, fees.begin(), [](const auto& el) {
    return ComputeWithdrawalFeesFromNetworkList(CurrencyCode{el.coin}, el);
  });

  log::info("Retrieved {} withdrawal fees for binance", fees.size());
  return MonetaryAmountByCurrencySet(std::move(fees));
}

MonetaryAmount BinanceGlobalInfos::queryWithdrawalFee(CurrencyCode currencyCode) {
  std::lock_guard<std::mutex> guard(_mutex);
  const auto& allCoins = _globalInfosCache.get();
  const auto curStr = currencyCode.str();

  const auto it = std::ranges::partition_point(allCoins, [&curStr](const auto& el) { return el.coin < curStr; });
  if (it != allCoins.end() && it->coin == curStr) {
    return ComputeWithdrawalFeesFromNetworkList(currencyCode, *it);
  }
  return MonetaryAmount(0, currencyCode);
}

CurrencyExchangeFlatSet BinanceGlobalInfos::queryTradableCurrencies(const CurrencyCodeSet& excludedCurrencies) {
  std::lock_guard<std::mutex> guard(_mutex);
  return ExtractTradableCurrencies(_globalInfosCache.get(), excludedCurrencies);
}

CurrencyExchangeFlatSet BinanceGlobalInfos::ExtractTradableCurrencies(
    const schema::binance::NetworkCoinDataVector& networkCoinDataVector, const CurrencyCodeSet& excludedCurrencies) {
  CurrencyExchangeVector currencies;
  for (const auto& coinJson : networkCoinDataVector) {
    CurrencyCode cur{coinJson.coin};
    if (excludedCurrencies.contains(cur)) {
      log::trace("Discard {} excluded by config", cur.str());
      continue;
    }
    const auto& networkList = coinJson.networkList;
    if (coinJson.networkList.size() > 1) {
      log::debug("Several networks found for {}, considering only default network", cur.str());
    }
    const auto it = std::ranges::find_if(networkList, [](const auto& el) { return el.isDefault; });
    if (it != networkList.end()) {
      auto deposit =
          it->depositEnable ? CurrencyExchange::Deposit::kAvailable : CurrencyExchange::Deposit::kUnavailable;
      auto withdraw =
          it->withdrawEnable ? CurrencyExchange::Withdraw::kAvailable : CurrencyExchange::Withdraw::kUnavailable;

      currencies.emplace_back(cur, cur, cur, deposit, withdraw,
                              coinJson.isLegalMoney ? CurrencyExchange::Type::kFiat : CurrencyExchange::Type::kCrypto);
    }
  }
  CurrencyExchangeFlatSet ret(std::move(currencies));
  log::info("Retrieved {} currencies from binance", ret.size());
  return ret;
}

}  // namespace cct::api