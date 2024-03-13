#include "withdrawalfees-crawler.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>

#include "cachedresult.hpp"
#include "cachedresultvault.hpp"
#include "cct_cctype.hpp"
#include "cct_const.hpp"
#include "cct_exception.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "cct_string.hpp"
#include "coincenterinfo.hpp"
#include "curloptions.hpp"
#include "currencycode.hpp"
#include "file.hpp"
#include "httprequesttype.hpp"
#include "monetaryamount.hpp"
#include "permanentcurloptions.hpp"
#include "timedef.hpp"

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
  json data = GetWithdrawInfoFile(_coincenterInfo.dataDir()).readAllJson();
  if (!data.empty()) {
    const auto nowTime = Clock::now();
    for (const auto& [exchangeName, exchangeData] : data.items()) {
      TimePoint lastUpdatedTime(seconds(exchangeData["timeepoch"].get<int64_t>()));
      if (nowTime - lastUpdatedTime < minDurationBetweenQueries) {
        // we can reuse file data
        WithdrawalInfoMaps withdrawalInfoMaps;

        for (const auto& [curCodeStr, val] : exchangeData["assets"].items()) {
          CurrencyCode cur(curCodeStr);
          MonetaryAmount withdrawMin(val["min"].get<std::string_view>(), cur);
          MonetaryAmount withdrawFee(val["fee"].get<std::string_view>(), cur);

          log::trace("Updated {} withdrawal fee {} from cache", exchangeName, withdrawFee);
          log::trace("Updated {} min withdraw {} from cache", exchangeName, withdrawMin);

          withdrawalInfoMaps.first.insert(withdrawFee);
          withdrawalInfoMaps.second.insert_or_assign(cur, withdrawMin);
        }

        _withdrawalFeesCache.set(std::move(withdrawalInfoMaps), lastUpdatedTime, exchangeName);
      }
    }
  }
}

WithdrawalFeesCrawler::WithdrawalFeesFunc::WithdrawalFeesFunc(const CoincenterInfo& coincenterInfo)
    : _curlHandle1(kUrlWithdrawFee1, coincenterInfo.metricGatewayPtr(), PermanentCurlOptions(),
                   coincenterInfo.getRunMode()),
      _curlHandle2(kUrlWithdrawFee2, coincenterInfo.metricGatewayPtr(), PermanentCurlOptions(),
                   coincenterInfo.getRunMode()) {}

WithdrawalFeesCrawler::WithdrawalInfoMaps WithdrawalFeesCrawler::WithdrawalFeesFunc::operator()(
    std::string_view exchangeName) {
  auto [withdrawFees1, withdrawMinMap1] = get1(exchangeName);
  auto [withdrawFees2, withdrawMinMap2] = get2(exchangeName);

  withdrawFees1.insert(withdrawFees2.begin(), withdrawFees2.end());
  withdrawMinMap1.merge(std::move(withdrawMinMap2));

  if (withdrawFees1.empty() || withdrawMinMap1.empty()) {
    log::error("Unable to parse {} withdrawal fees", exchangeName);
  }

  return std::make_pair(std::move(withdrawFees1), std::move(withdrawMinMap1));
}

void WithdrawalFeesCrawler::updateCacheFile() const {
  json data;
  for (const std::string_view exchangeName : kSupportedExchanges) {
    const auto [withdrawalInfoMapsPtr, latestUpdate] = _withdrawalFeesCache.retrieve(exchangeName);
    if (withdrawalInfoMapsPtr != nullptr) {
      const WithdrawalInfoMaps& withdrawalInfoMaps = *withdrawalInfoMapsPtr;

      json exchangeData;
      exchangeData["timeepoch"] = TimestampToSecondsSinceEpoch(latestUpdate);
      for (const auto withdrawFee : withdrawalInfoMaps.first) {
        string curCodeStr = withdrawFee.currencyCode().str();
        exchangeData["assets"][curCodeStr]["min"] =
            withdrawalInfoMaps.second.find(withdrawFee.currencyCode())->second.amountStr();
        exchangeData["assets"][curCodeStr]["fee"] = withdrawFee.amountStr();
      }

      data.emplace(exchangeName, std::move(exchangeData));
    }
  }
  GetWithdrawInfoFile(_coincenterInfo.dataDir()).write(data);
}

WithdrawalFeesCrawler::WithdrawalInfoMaps WithdrawalFeesCrawler::WithdrawalFeesFunc::get1(
    std::string_view exchangeName) {
  string path(exchangeName);
  path.append(".json");
  std::string_view withdrawalFeesCsv = _curlHandle1.query(path, CurlOptions(HttpRequestType::kGet));

  WithdrawalInfoMaps ret;

  if (!withdrawalFeesCsv.empty()) {
    const json jsonData = json::parse(withdrawalFeesCsv);
    const auto exchangesIt = jsonData.find("exchange");
    if (exchangesIt == jsonData.end()) {
      log::error("no exchange data found in source 1 - either site information unavailable or code to be updated");
      return ret;
    }
    const auto feesIt = exchangesIt->find("fees");
    if (feesIt == exchangesIt->end() || !feesIt->is_array()) {
      log::error("no fees data found in source 1 - either site information unavailable or code to be updated");
      return ret;
    }

    for (const json& feeJson : *feesIt) {
      const auto amountIt = feeJson.find("amount");
      if (amountIt == feeJson.end() || !amountIt->is_number_float()) {
        continue;
      }

      const auto coinIt = feeJson.find("coin");
      if (coinIt == feeJson.end()) {
        continue;
      }
      const auto symbolIt = coinIt->find("symbol");
      if (symbolIt == coinIt->end() || !symbolIt->is_string()) {
        continue;
      }

      MonetaryAmount withdrawalFee(amountIt->get<double>(), symbolIt->get<std::string_view>());
      log::trace("Updated {} withdrawal fee {} from first source", exchangeName, withdrawalFee);
      ret.first.insert(withdrawalFee);

      const auto minWithdrawalIt = feeJson.find("min");
      if (minWithdrawalIt == feeJson.end() || !minWithdrawalIt->is_number_float()) {
        continue;
      }

      MonetaryAmount minWithdrawal(minWithdrawalIt->get<double>(), symbolIt->get<std::string_view>());

      log::trace("Updated {} min withdrawal {} from first source", exchangeName, minWithdrawal);
      ret.second.insert_or_assign(minWithdrawal.currencyCode(), minWithdrawal);
    }
  }

  if (ret.first.empty() || ret.second.empty()) {
    log::warn("Unable to parse {} withdrawal fees from first source", exchangeName);
  } else {
    log::info("Updated {} withdraw infos for {} coins from first source", exchangeName, ret.first.size());
  }
  return ret;
}

WithdrawalFeesCrawler::WithdrawalInfoMaps WithdrawalFeesCrawler::WithdrawalFeesFunc::get2(
    std::string_view exchangeName) {
  std::string_view withdrawalFeesCsv = _curlHandle2.query(exchangeName, CurlOptions(HttpRequestType::kGet));

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
          MonetaryAmount ret(std::string_view(withdrawalFeesCsv.begin() + begPos, withdrawalFeesCsv.begin() + endPos));
          begPos = endHtmlTagPos + kEndHtmlTag.size();
          return ret;
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