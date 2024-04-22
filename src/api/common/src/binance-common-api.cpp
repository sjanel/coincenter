#include "binance-common-api.hpp"

#include <algorithm>
#include <iterator>
#include <mutex>
#include <string_view>
#include <utility>

#include "abstractmetricgateway.hpp"
#include "cachedresult.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "cct_string.hpp"
#include "curlhandle.hpp"
#include "curlpostdata.hpp"
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

json PublicQuery(CurlHandle& curlHandle, std::string_view method) {
  RequestRetry requestRetry(curlHandle, CurlOptions(HttpRequestType::kGet));

  return requestRetry.queryJson(method, [](const json& jsonResponse) {
    const auto foundErrorIt = jsonResponse.find("code");
    const auto foundMsgIt = jsonResponse.find("msg");
    if (foundErrorIt != jsonResponse.end() && foundMsgIt != jsonResponse.end()) {
      const int statusCode = foundErrorIt->get<int>();  // "1100" for instance
      log::warn("Binance error ({}), full json: '{}'", statusCode, jsonResponse.dump());
      return RequestRetry::Status::kResponseError;
    }
    return RequestRetry::Status::kResponseOK;
  });
}

constexpr std::string_view kCryptoFeeBaseUrl = "https://www.binance.com";
}  // namespace

BinanceGlobalInfosFunc::BinanceGlobalInfosFunc(AbstractMetricGateway* pMetricGateway,
                                               const PermanentCurlOptions& permanentCurlOptions,
                                               settings::RunMode runMode)
    : _curlHandle(kCryptoFeeBaseUrl, pMetricGateway, permanentCurlOptions, runMode) {}

json BinanceGlobalInfosFunc::operator()() {
  json ret = PublicQuery(_curlHandle, "/bapi/capital/v1/public/capital/getNetworkCoinAll");
  auto dataIt = ret.find("data");
  json dataRet;
  if (dataIt == ret.end() || !dataIt->is_array()) {
    log::error("Unexpected reply from binance getNetworkCoinAll, no data array");
    dataRet = json::array_t();
  } else {
    dataRet = std::move(*dataIt);
  }

  const auto endIt = std::remove_if(dataRet.begin(), dataRet.end(), [](const json& el) {
    return el["coin"].get<std::string_view>().size() > CurrencyCode::kMaxLen;
  });

  if (endIt != dataRet.end()) {
    log::debug("{} currencies discarded for binance as code too long", dataRet.end() - endIt);
    dataRet.erase(endIt, dataRet.end());
  }

  std::sort(dataRet.begin(), dataRet.end(), [](const json& lhs, const json& rhs) {
    return lhs["coin"].get<std::string_view>() < rhs["coin"].get<std::string_view>();
  });
  return dataRet;
}

namespace {
MonetaryAmount ComputeWithdrawalFeesFromNetworkList(CurrencyCode cur, const json& coinElem) {
  MonetaryAmount withdrawFee(0, cur);
  auto networkListIt = coinElem.find("networkList");
  if (networkListIt == coinElem.end()) {
    log::error("Unexpected Binance public coin data format, returning 0 monetary amount");
    return withdrawFee;
  }
  for (const json& networkListPart : *networkListIt) {
    MonetaryAmount fee(networkListPart["withdrawFee"].get<std::string_view>(), cur);
    auto isDefaultIt = networkListPart.find("isDefault");
    if (isDefaultIt != networkListPart.end() && isDefaultIt->get<bool>()) {
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

  MonetaryAmountVector fees;

  fees.reserve(allCoins.size());

  std::transform(allCoins.begin(), allCoins.end(), std::back_inserter(fees), [](const json& el) {
    CurrencyCode cur(el["coin"].get<std::string_view>());
    return ComputeWithdrawalFeesFromNetworkList(cur, el);
  });

  log::info("Retrieved {} withdrawal fees for binance", fees.size());
  return MonetaryAmountByCurrencySet(std::move(fees));
}

MonetaryAmount BinanceGlobalInfos::queryWithdrawalFee(CurrencyCode currencyCode) {
  std::lock_guard<std::mutex> guard(_mutex);
  const auto& allCoins = _globalInfosCache.get();
  const auto curStr = currencyCode.str();

  const auto it = std::partition_point(allCoins.begin(), allCoins.end(), [&curStr](const json& el) {
    return el["coin"].get<std::string_view>() < curStr;
  });
  if (it != allCoins.end() && (*it)["coin"].get<std::string_view>() == curStr) {
    return ComputeWithdrawalFeesFromNetworkList(currencyCode, *it);
  }
  return MonetaryAmount(0, currencyCode);
}

CurrencyExchangeFlatSet BinanceGlobalInfos::queryTradableCurrencies(const CurrencyCodeSet& excludedCurrencies) {
  std::lock_guard<std::mutex> guard(_mutex);
  return ExtractTradableCurrencies(_globalInfosCache.get(), excludedCurrencies);
}

CurrencyExchangeFlatSet BinanceGlobalInfos::ExtractTradableCurrencies(const json& allCoins,
                                                                      const CurrencyCodeSet& excludedCurrencies) {
  CurrencyExchangeVector currencies;
  for (const json& coinJson : allCoins) {
    CurrencyCode cur(coinJson["coin"].get<std::string_view>());
    if (excludedCurrencies.contains(cur)) {
      log::trace("Discard {} excluded by config", cur.str());
      continue;
    }
    const bool isFiat = coinJson["isLegalMoney"];
    const auto& networkList = coinJson["networkList"];
    if (networkList.size() > 1) {
      log::debug("Several networks found for {}, considering only default network", cur.str());
    }
    const auto it = std::find_if(networkList.begin(), networkList.end(),
                                 [](const json& el) { return el["isDefault"].get<bool>(); });
    if (it != networkList.end()) {
      auto deposit = (*it)["depositEnable"].get<bool>() ? CurrencyExchange::Deposit::kAvailable
                                                        : CurrencyExchange::Deposit::kUnavailable;
      auto withdraw = (*it)["withdrawEnable"].get<bool>() ? CurrencyExchange::Withdraw::kAvailable
                                                          : CurrencyExchange::Withdraw::kUnavailable;

      currencies.emplace_back(cur, cur, cur, deposit, withdraw,
                              isFiat ? CurrencyExchange::Type::kFiat : CurrencyExchange::Type::kCrypto);
    }
  }
  CurrencyExchangeFlatSet ret(std::move(currencies));
  log::info("Retrieved {} currencies from binance", ret.size());
  return ret;
}

}  // namespace cct::api