#include "commonapi.hpp"

#include <glaze/glaze.hpp>  // IWYU pragma: export
#include <mutex>
#include <optional>
#include <string_view>
#include <utility>

#include "cachedresult.hpp"
#include "cct_const.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "cct_string.hpp"
#include "cct_vector.hpp"
#include "coincenterinfo.hpp"
#include "curloptions.hpp"
#include "currencycode.hpp"
#include "currencycodeset.hpp"
#include "currencycodevector.hpp"
#include "fiats-cache-schema.hpp"
#include "file.hpp"
#include "httprequesttype.hpp"
#include "monetaryamountbycurrencyset.hpp"
#include "permanentcurloptions.hpp"
#include "read-json.hpp"
#include "timedef.hpp"
#include "withdrawalfees-crawler.hpp"
#include "write-json.hpp"

namespace cct::api {
namespace {

File GetFiatCacheFile(std::string_view dataDir) {
  return {dataDir, File::Type::kCache, "fiatcache.json", File::IfError::kNoThrow};
}

}  // namespace

CommonAPI::CommonAPI(const CoincenterInfo& coincenterInfo, Duration fiatsUpdateFrequency,
                     Duration withdrawalFeesUpdateFrequency, AtInit atInit)
    : _coincenterInfo(coincenterInfo),
      _fiatsCache(CachedResultOptions(fiatsUpdateFrequency, _cachedResultVault), coincenterInfo),
      _binanceGlobalInfos(CachedResultOptions(fiatsUpdateFrequency, _cachedResultVault),
                          coincenterInfo.metricGatewayPtr(),
                          PermanentCurlOptions::Builder()
                              .setFollowLocation()
                              .setAcceptedEncoding(kDefaultAcceptEncoding)
                              .setTooManyErrorsPolicy(PermanentCurlOptions::TooManyErrorsPolicy::kReturnEmptyResponse)
                              .build(),
                          coincenterInfo.getRunMode()),
      _withdrawalFeesCrawler(coincenterInfo, withdrawalFeesUpdateFrequency, _cachedResultVault) {
  if (atInit == AtInit::kLoadFromFileCache) {
    schema::FiatsCache fiatsCache;
    auto dataStr = GetFiatCacheFile(_coincenterInfo.dataDir()).readAll();
    if (!dataStr.empty()) {
      ReadExactJsonOrThrow(dataStr, fiatsCache);
      if (fiatsCache.timeepoch != 0) {
        CurrencyCodeSet fiats(std::move(fiatsCache.fiats));
        log::debug("Loaded {} fiats from cache file", fiats.size());
        _fiatsCache.set(std::move(fiats), TimePoint(seconds(fiatsCache.timeepoch)));
      }
    }
  }
}

CurrencyCodeSet CommonAPI::queryFiats() {
  std::lock_guard<std::recursive_mutex> guard(_globalMutex);
  return _fiatsCache.get();
}

bool CommonAPI::queryIsCurrencyCodeFiat(CurrencyCode currencyCode) { return queryFiats().contains(currencyCode); }

MonetaryAmountByCurrencySet CommonAPI::tryQueryWithdrawalFees(ExchangeNameEnum exchangeNameEnum) {
  MonetaryAmountByCurrencySet ret;
  {
    std::lock_guard<std::recursive_mutex> guard(_globalMutex);
    ret = _withdrawalFeesCrawler.get(exchangeNameEnum).first;
  }

  if (ret.empty()) {
    log::warn("Taking binance withdrawal fees for {} as crawler failed to retrieve data",
              EnumToString(exchangeNameEnum));
    ret = _binanceGlobalInfos.queryWithdrawalFees();
  }
  return ret;
}

std::optional<MonetaryAmount> CommonAPI::tryQueryWithdrawalFee(ExchangeNameEnum exchangeNameEnum,
                                                               CurrencyCode currencyCode) {
  {
    std::lock_guard<std::recursive_mutex> guard(_globalMutex);
    const auto& withdrawalFees = _withdrawalFeesCrawler.get(exchangeNameEnum).first;
    auto it = withdrawalFees.find(currencyCode);
    if (it != withdrawalFees.end()) {
      return *it;
    }
  }
  log::warn("Taking binance withdrawal fee for {} and currency {} as crawler failed to retrieve data",
            EnumToString(exchangeNameEnum), currencyCode);
  MonetaryAmount withdrawFee = _binanceGlobalInfos.queryWithdrawalFee(currencyCode);
  if (withdrawFee.isDefault()) {
    return {};
  }
  return withdrawFee;
}

namespace {
constexpr std::string_view kFiatsUrlSource1 = "https://datahub.io/core/currency-codes/_r/-/data/codes-all.csv";
constexpr std::string_view kFiatsUrlSource2 = "https://www.iban.com/currency-codes";
}  // namespace

CommonAPI::FiatsFunc::FiatsFunc(const CoincenterInfo& coincenterInfo)
    : _curlHandle1(kFiatsUrlSource1, coincenterInfo.metricGatewayPtr(),
                   PermanentCurlOptions::Builder()
                       .setFollowLocation()
                       .setTooManyErrorsPolicy(PermanentCurlOptions::TooManyErrorsPolicy::kReturnEmptyResponse)
                       .build()),
      _curlHandle2(kFiatsUrlSource2, coincenterInfo.metricGatewayPtr(),
                   PermanentCurlOptions::Builder()
                       .setTooManyErrorsPolicy(PermanentCurlOptions::TooManyErrorsPolicy::kReturnEmptyResponse)
                       .build()) {}

CurrencyCodeSet CommonAPI::FiatsFunc::operator()() {
  CurrencyCodeVector fiatsVec = retrieveFiatsSource1();
  CurrencyCodeSet fiats;
  if (!fiatsVec.empty()) {
    fiats = CurrencyCodeSet(std::move(fiatsVec));
    log::info("Stored {} fiats from first source", fiats.size());
  } else {
    fiats = CurrencyCodeSet(retrieveFiatsSource2());
    log::info("Stored {} fiats from second source", fiats.size());
  }
  return fiats;
}
struct CurrencyCSV {
  vector<string> Entity;
  vector<string> Currency;
  vector<string> AlphabeticCode;
  vector<string> NumericCode;
  vector<string> MinorUnit;
  vector<string> WithdrawalDate;
};

CurrencyCodeVector CommonAPI::FiatsFunc::retrieveFiatsSource1() {
  CurrencyCodeVector fiatsVec;

  std::string_view data = _curlHandle1.query("", CurlOptions(HttpRequestType::kGet));
  if (data.empty()) {
    log::warn("Error parsing currency codes, no fiats found from first source");
    return fiatsVec;
  }

  // data is UTF-8 encoded - but the relevant data that we will parse is ASCII normally

  CurrencyCSV currencies;
  auto ec = json::read<json::opts{.format = json::CSV, .layout = json::colwise}>(currencies, data);

  if (ec || currencies.AlphabeticCode.size() != currencies.WithdrawalDate.size()) {
    log::warn("Error parsing json data of currency codes from source 1: {}", json::format_error(ec, data));
    return fiatsVec;
  }

  auto nbCurrencies = currencies.AlphabeticCode.size();
  for (decltype(nbCurrencies) currencyPos = 0; currencyPos < nbCurrencies; ++currencyPos) {
    if (currencies.WithdrawalDate[currencyPos].empty() && !currencies.AlphabeticCode[currencyPos].empty()) {
      fiatsVec.emplace_back(currencies.AlphabeticCode[currencyPos]);
      log::debug("Stored {} fiat", fiatsVec.back());
    }
  }

  log::info("Found {} fiats from first source", fiatsVec.size());

  return fiatsVec;
}

CurrencyCodeVector CommonAPI::FiatsFunc::retrieveFiatsSource2() {
  CurrencyCodeVector fiatsVec;
  std::string_view data = _curlHandle2.query("", CurlOptions(HttpRequestType::kGet));
  if (data.empty()) {
    log::error("Error parsing currency codes, no fiats found from second source");
    return fiatsVec;
  }

  static constexpr std::string_view kTheadColumnStart = "<th class=\"head\">";
  static constexpr std::string_view kCodeStrName = "Code";
  auto pos = data.find("<thead>");
  int codePos = 0;
  for (pos = data.find(kTheadColumnStart, pos + 1U); pos != std::string_view::npos;
       pos = data.find(kTheadColumnStart, pos + 1U), ++codePos) {
    auto endPos = data.find("</th>", pos + 1);
    std::string_view colName(data.begin() + pos + kTheadColumnStart.size(), data.begin() + endPos);
    if (colName == kCodeStrName) {
      ++codePos;
      break;
    }
    pos = endPos;
  }

  pos = data.find("<tbody>");
  for (pos = data.find("<tr>", pos + 1U); pos != std::string_view::npos; pos = data.find("<tr>", pos + 1U)) {
    static constexpr std::string_view kColStart = "<td>";
    for (int col = 0; col < codePos; ++col) {
      pos = data.find(kColStart, pos + 1);
    }
    auto endPos = data.find("</td>", pos + 1);
    std::string_view curStr(data.begin() + pos + kColStart.size(), data.begin() + endPos);
    if (!curStr.empty()) {
      // Fiat data is sometimes empty
      fiatsVec.emplace_back(curStr);
    }
  }

  return fiatsVec;
}

void CommonAPI::updateCacheFile() const {
  const auto fiatsCacheFile = GetFiatCacheFile(_coincenterInfo.dataDir());
  auto fiatsDataStr = fiatsCacheFile.readAll();
  schema::FiatsCache fiatsData;
  ReadExactJsonOrThrow(fiatsDataStr, fiatsData);
  const auto fiatsPtrLastUpdatedTimePair = _fiatsCache.retrieve();
  if (TimePoint(seconds(fiatsData.timeepoch)) < fiatsPtrLastUpdatedTimePair.second) {
    // update fiats cache file
    fiatsData.fiats.clear();
    if (fiatsPtrLastUpdatedTimePair.first != nullptr) {
      for (CurrencyCode fiatCode : *fiatsPtrLastUpdatedTimePair.first) {
        fiatsData.fiats.emplace_back(fiatCode.str());
      }
      fiatsData.timeepoch = TimestampToSecondsSinceEpoch(fiatsPtrLastUpdatedTimePair.second);
      fiatsCacheFile.write(WriteJsonOrThrow(fiatsData));
    }
  }

  _withdrawalFeesCrawler.updateCacheFile();
}
}  // namespace cct::api
