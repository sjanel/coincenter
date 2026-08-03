#include "withdrawalfees-crawler.hpp"

#include <cstddef>
#include <string_view>
#include <utility>

#include "cachedresult.hpp"
#include "cachedresultvault.hpp"
#include "cct_cctype.hpp"
#include "cct_exception.hpp"
#include "cct_log.hpp"
#include "cct_string.hpp"
#include "coincenterinfo.hpp"
#include "httprequestoptions.hpp"
#include "currencycode.hpp"
#include "enum-string.hpp"
#include "exchange-name-enum.hpp"
#include "file.hpp"
#include "httprequesttype.hpp"
#include "monetaryamount.hpp"
#include "permanentrequestoptions.hpp"
#include "read-json.hpp"
#include "timedef.hpp"
#include "withdrawal-fees-schema.hpp"
#include "write-json.hpp"

namespace cct {

namespace {
constexpr std::string_view kUrlWithdrawFee1 = "https://withdrawalfees.com/exchanges/";
constexpr std::string_view kUrlWithdrawFee2 = "https://www.cryptofeesaver.com/exchanges/fees/";

File GetWithdrawInfoFile(std::string_view dataDir) {
  return {dataDir, File::Type::kCache, "withdrawinfo.json", File::IfError::kNoThrow};
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
                      .setTooManyErrorsPolicy(PermanentRequestOptions::TooManyErrorsPolicy::kReturnEmptyResponse)
                      .build(),
                  coincenterInfo.getRunMode()) {}

WithdrawalFeesCrawler::WithdrawalInfoMaps WithdrawalFeesCrawler::WithdrawalFeesFunc::operator()(
    ExchangeNameEnum exchangeNameEnum) {
  // The two sources are fetched sequentially: they now share a single HttpClient (aeronet's client is not
  // bound to a single host), whose response buffer is reused across queries and is therefore not safe to
  // query concurrently.
  auto [withdrawFees, withdrawMinMap] = get1(exchangeNameEnum);

  auto [withdrawFees2, withdrawMinMap2] = get2(exchangeNameEnum);
  withdrawFees.insert(withdrawFees2.begin(), withdrawFees2.end());
  withdrawMinMap.merge(std::move(withdrawMinMap2));

  if (withdrawFees.empty() || withdrawMinMap.empty()) {
    log::error("Unable to parse {} withdrawal fees", EnumToString(exchangeNameEnum));
  }

  return std::make_pair(std::move(withdrawFees), std::move(withdrawMinMap));
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

WithdrawalFeesCrawler::WithdrawalInfoMaps WithdrawalFeesCrawler::WithdrawalFeesFunc::get1(
    ExchangeNameEnum exchangeNameEnum) {
  std::string_view exchangeName = EnumToString(exchangeNameEnum);
  string url(kUrlWithdrawFee1);
  url.append(exchangeName);
  url.append(".json");
  std::string_view dataStr = _httpClient.query(url, HttpRequestOptions(HttpRequestType::kGet));

  WithdrawalInfoMaps ret;

  schema::WithdrawFeesCrawlerSource1 withdrawalFeesCrawlerSource1;
  ReadPartialJson(dataStr, "withdraw fees crawler service's first source", withdrawalFeesCrawlerSource1);

  if (withdrawalFeesCrawlerSource1.exchange.fees.empty()) {
    log::error("no fees data found in source 1 - either site information unavailable or code to be updated");
    return ret;
  }

  for (const schema::WithdrawFeesCrawlerExchangeFeesSource1& fee : withdrawalFeesCrawlerSource1.exchange.fees) {
    if (fee.coin.symbol.size() > CurrencyCode::kMaxLen) {
      log::warn("Skipping {} withdrawal fees parsing from first source: symbol too long", fee.coin.symbol);
      continue;
    }

    CurrencyCode cur{fee.coin.symbol};

    MonetaryAmount withdrawalFee(fee.amount, cur);
    log::trace("Updated {} withdrawal fee {} from first source", exchangeName, withdrawalFee);
    ret.first.insert(withdrawalFee);

    MonetaryAmount minWithdrawal(fee.min, cur);

    log::trace("Updated {} min withdrawal {} from first source", exchangeName, minWithdrawal);
    ret.second.insert_or_assign(minWithdrawal.currencyCode(), minWithdrawal);
  }

  if (ret.first.empty() || ret.second.empty()) {
    log::warn("Unable to parse {} withdrawal fees from first source", exchangeName);
  } else {
    log::info("Updated {} withdraw infos for {} coins from first source", exchangeName, ret.first.size());
  }
  return ret;
}

WithdrawalFeesCrawler::WithdrawalInfoMaps WithdrawalFeesCrawler::WithdrawalFeesFunc::get2(
    ExchangeNameEnum exchangeNameEnum) {
  std::string_view exchangeName = EnumToString(exchangeNameEnum);
  string url(kUrlWithdrawFee2);
  url.append(exchangeName);
  std::string_view withdrawalFeesCsv = _httpClient.query(url, HttpRequestOptions(HttpRequestType::kGet));

  static constexpr std::string_view kBeginTableTitle = "Deposit & Withdrawal fees</h2>";

  std::size_t begPos = withdrawalFeesCsv.find(kBeginTableTitle);
  WithdrawalInfoMaps ret;
  if (begPos != string::npos) {
    static constexpr std::string_view kBeginTable = "<table class=";
    begPos = withdrawalFeesCsv.find(kBeginTable, begPos + kBeginTableTitle.size());
    if (begPos != string::npos) {
      static constexpr std::string_view kBeginWithdrawalFeeHtmlTag = R"(<th scope="row" class="align)";

      std::size_t searchPos = begPos + kBeginTable.size();
      while ((searchPos = withdrawalFeesCsv.find(kBeginWithdrawalFeeHtmlTag, searchPos)) != string::npos) {
        auto parseNextFee = [exchangeName, &withdrawalFeesCsv](std::size_t& begPos) -> MonetaryAmount {
          static constexpr std::string_view kBeginFeeHtmlTag = "<td class=\"align-middle align-right\">";
          static constexpr std::string_view kEndHtmlTag = "</td>";

          // Skip one column
          for (int colPos = 0; colPos < 2; ++colPos) {
            begPos = withdrawalFeesCsv.find(kBeginFeeHtmlTag, begPos);
            if (begPos == string::npos) {
              throw exception("Unable to parse {} withdrawal fees from source 2: expecting begin HTML tag",
                              exchangeName);
            }
            begPos += kBeginFeeHtmlTag.size();
          }
          // Scan until next non space char
          while (begPos < withdrawalFeesCsv.size() && isspace(withdrawalFeesCsv[begPos])) {
            ++begPos;
          }
          std::size_t endPos = withdrawalFeesCsv.find(kEndHtmlTag, begPos + 1);
          if (endPos == string::npos) {
            throw exception("Unable to parse {} withdrawal fees from source 2: expecting end HTML tag", exchangeName);
          }
          std::size_t endHtmlTagPos = endPos;
          while (endPos > begPos && isspace(withdrawalFeesCsv[endPos - 1])) {
            --endPos;
          }
          MonetaryAmount amt(std::string_view(withdrawalFeesCsv.begin() + begPos, withdrawalFeesCsv.begin() + endPos));
          begPos = endHtmlTagPos + kEndHtmlTag.size();
          return amt;
        };

        // Locate withdrawal fee
        searchPos += kBeginWithdrawalFeeHtmlTag.size();
        MonetaryAmount withdrawalFee = parseNextFee(searchPos);

        log::trace("Updated {} withdrawal fee {} from source 2, simulate min withdrawal amount", exchangeName,
                   withdrawalFee);
        ret.first.insert(withdrawalFee);

        ret.second.insert_or_assign(withdrawalFee.currencyCode(), 3 * withdrawalFee);
      }
    }
  }

  if (ret.first.empty() || ret.second.empty()) {
    log::warn("Unable to parse {} withdrawal fees from second source", exchangeName);
  } else {
    log::info("Updated {} withdraw infos for {} coins from second source", exchangeName, ret.first.size());
  }
  return ret;
}

}  // namespace cct