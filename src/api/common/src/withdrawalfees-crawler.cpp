#include "withdrawalfees-crawler.hpp"

#include <string_view>
#include <utility>

#include "cachedresult.hpp"
#include "cachedresultvault.hpp"
#include "cct_log.hpp"
#include "cct_string.hpp"
#include "coincenterinfo.hpp"
#include "currencycode.hpp"
#include "enum-string.hpp"
#include "exchange-name-enum.hpp"
#include "file.hpp"
#include "httprequestoptions.hpp"
#include "httprequesttype.hpp"
#include "monetaryamount.hpp"
#include "permanentrequestoptions.hpp"
#include "read-json.hpp"
#include "timedef.hpp"
#include "withdrawal-fees-schema.hpp"
#include "write-json.hpp"

namespace cct {

namespace {
constexpr std::string_view kBithumbWithdrawalFeesUrl = "https://api.bithumb.com/v2/fee/inout/ALL";
constexpr std::string_view kKrakenWithdrawalFeesUrl =
    "https://iapi.kraken.com/api/internal/withdrawals/public/methods?preferred_asset_name=new";

File GetWithdrawInfoFile(std::string_view dataDir) {
  return {dataDir, File::Type::kCache, "withdrawinfo.json", File::IfError::kNoThrow};
}

void InsertConservativeWithdrawalInfo(WithdrawalFeesCrawler::WithdrawalInfoMaps& withdrawalInfoMaps,
                                      CurrencyCode currencyCode, std::string_view feeAmount,
                                      std::string_view minAmount) {
  if (feeAmount.empty()) {
    return;
  }

  MonetaryAmount withdrawalFee(feeAmount, currencyCode);
  const auto feeIt = withdrawalInfoMaps.first.find(currencyCode);
  if (feeIt != withdrawalInfoMaps.first.end() && withdrawalFee < *feeIt) {
    return;
  }

  withdrawalInfoMaps.first.insert_or_assign(withdrawalFee);
  if (!minAmount.empty()) {
    withdrawalInfoMaps.second.insert_or_assign(currencyCode, MonetaryAmount(minAmount, currencyCode));
  }
}
}  // namespace

WithdrawalFeesCrawler::WithdrawalFeesCrawler(const CoincenterInfo& coincenterInfo, Duration minDurationBetweenQueries,
                                             CachedResultVault& cachedResultVault)
    : _coincenterInfo(coincenterInfo),
      _withdrawalFeesCache(CachedResultOptions(minDurationBetweenQueries, cachedResultVault), coincenterInfo) {
  auto data = GetWithdrawInfoFile(_coincenterInfo.dataDir()).readAll();

  schema::WithdrawInfoFile withdrawInfoFileContent;

  ReadExactJsonOrThrow(data, withdrawInfoFileContent);

  const auto nowTime = Clock::now();
  for (const auto& [exchangeNameEnum, exchangeData] : withdrawInfoFileContent) {
    TimePoint lastUpdatedTime(seconds(exchangeData.timeepoch));
    if (nowTime - lastUpdatedTime < minDurationBetweenQueries) {
      // we can reuse file data
      WithdrawalInfoMaps withdrawalInfoMaps;

      std::string_view exchangeName = EnumToString(exchangeNameEnum);

      for (const auto& [cur, val] : exchangeData.assets) {
        MonetaryAmount withdrawMin(val.min, cur);
        MonetaryAmount withdrawFee(val.fee, cur);

        log::trace("Updated {} withdrawal fee {} from cache", exchangeName, withdrawFee);
        log::trace("Updated {} min withdraw {} from cache", exchangeName, withdrawMin);

        withdrawalInfoMaps.first.insert(withdrawFee);
        withdrawalInfoMaps.second.insert_or_assign(cur, withdrawMin);
      }

      _withdrawalFeesCache.set(std::move(withdrawalInfoMaps), lastUpdatedTime, exchangeNameEnum);
    }
  }
}

WithdrawalFeesCrawler::WithdrawalFeesFunc::WithdrawalFeesFunc(const CoincenterInfo& coincenterInfo)
    : _httpClient(kNoBaseUrl, coincenterInfo.metricGatewayPtr(),
                  PermanentRequestOptions::Builder()
                      .setFollowLocation()
                      .setTooManyErrorsPolicy(PermanentRequestOptions::TooManyErrorsPolicy::kReturnEmptyResponse)
                      .build(),
                  coincenterInfo.getRunMode()) {}

WithdrawalFeesCrawler::WithdrawalInfoMaps WithdrawalFeesCrawler::WithdrawalFeesFunc::operator()(
    ExchangeNameEnum exchangeNameEnum) {
  std::string_view response;
  WithdrawalInfoMaps withdrawalInfoMaps;
  switch (exchangeNameEnum) {
    case ExchangeNameEnum::bithumb:
      response = _httpClient.query(kBithumbWithdrawalFeesUrl, HttpRequestOptions(HttpRequestType::kGet));
      withdrawalInfoMaps = ParseBithumbResponse(response);
      break;
    case ExchangeNameEnum::kraken: {
      HttpRequestOptions requestOptions(HttpRequestType::kGet);
      auto& httpHeaders = requestOptions.mutableHttpHeaders();
      httpHeaders.emplace_back("Referer", "https://www.kraken.com/");
      httpHeaders.emplace_back("X-Kraken-Asset-Name", "new");
      response = _httpClient.query(kKrakenWithdrawalFeesUrl, requestOptions);
      withdrawalInfoMaps = ParseKrakenResponse(response);
      break;
    }
    default:
      // Other exchanges expose withdrawal fees through their own API implementation.
      return withdrawalInfoMaps;
  }

  if (withdrawalInfoMaps.first.empty()) {
    log::error("Unable to parse {} withdrawal fees", EnumToString(exchangeNameEnum));
  } else {
    log::info("Updated {} withdraw infos for {} coins", EnumToString(exchangeNameEnum),
              withdrawalInfoMaps.first.size());
  }

  return withdrawalInfoMaps;
}

void WithdrawalFeesCrawler::updateCacheFile() const {
  schema::WithdrawInfoFile withdrawInfoFile;
  for (int exchangeNamePos = 0; exchangeNamePos < kNbSupportedExchanges; ++exchangeNamePos) {
    auto exchangeNameEnum = static_cast<ExchangeNameEnum>(exchangeNamePos);
    const auto [withdrawalInfoMapsPtr, latestUpdate] = _withdrawalFeesCache.retrieve(exchangeNameEnum);
    if (withdrawalInfoMapsPtr != nullptr) {
      const WithdrawalInfoMaps& withdrawalInfoMaps = *withdrawalInfoMapsPtr;

      schema::WithdrawInfoFileItem& withdrawInfoFileItem =
          withdrawInfoFile.emplace(std::make_pair(exchangeNameEnum, schema::WithdrawInfoFileItem{})).first->second;
      withdrawInfoFileItem.timeepoch = TimestampToSecondsSinceEpoch(latestUpdate);
      for (const auto withdrawFee : withdrawalInfoMaps.first) {
        CurrencyCode cur = withdrawFee.currencyCode();

        schema::WithdrawInfoFileItemAsset& asset = withdrawInfoFileItem.assets[cur];

        auto minIt = withdrawalInfoMaps.second.find(cur);
        if (minIt != withdrawalInfoMaps.second.end()) {
          asset.min = MonetaryAmount(minIt->second, CurrencyCode{});
        }
        asset.fee = MonetaryAmount(withdrawFee, CurrencyCode{});
      }
    }
  }
  auto dataStr = WriteJsonOrThrow(withdrawInfoFile);

  GetWithdrawInfoFile(_coincenterInfo.dataDir()).write(dataStr);
}

WithdrawalFeesCrawler::WithdrawalInfoMaps WithdrawalFeesCrawler::ParseBithumbResponse(std::string_view dataStr) {
  WithdrawalInfoMaps ret;
  schema::BithumbWithdrawalFees response;
  if (ReadPartialJson(dataStr, "Bithumb withdrawal fees", response)) {
    return ret;
  }

  for (const schema::BithumbWithdrawalAsset& asset : response) {
    if (asset.currency.empty() || asset.currency.size() > CurrencyCode::kMaxLen) {
      log::warn("Skipping Bithumb withdrawal fee with invalid currency code '{}'", asset.currency);
      continue;
    }

    const CurrencyCode currencyCode(asset.currency);
    for (const schema::BithumbWithdrawalNetwork& network : asset.networks) {
      // Percentage fees cannot be represented by MonetaryAmount. Keep only fixed-fee networks.
      if (network.withdraw_fee_quantity) {
        InsertConservativeWithdrawalInfo(ret, currencyCode, *network.withdraw_fee_quantity,
                                         network.withdraw_minimum_quantity.value_or(string{}));
      }
    }
  }
  return ret;
}

WithdrawalFeesCrawler::WithdrawalInfoMaps WithdrawalFeesCrawler::ParseKrakenResponse(std::string_view dataStr) {
  WithdrawalInfoMaps ret;
  schema::KrakenWithdrawalMethodsResponse response;
  if (ReadPartialJson(dataStr, "Kraken withdrawal fees", response)) {
    return ret;
  }

  for (const schema::KrakenWithdrawalMethod& method : response.result) {
    if (method.asset.empty() || method.asset.size() > CurrencyCode::kMaxLen) {
      log::warn("Skipping Kraken withdrawal fee with invalid currency code '{}'", method.asset);
      continue;
    }

    InsertConservativeWithdrawalInfo(ret, CurrencyCode(method.asset), method.fee, method.min_amount);
  }
  return ret;
}

}  // namespace cct
