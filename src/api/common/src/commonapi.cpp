#include "commonapi.hpp"

#include <cstdint>
#include <mutex>
#include <string_view>
#include <utility>

#include "cachedresult.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "cct_string.hpp"
#include "cct_vector.hpp"
#include "coincenterinfo.hpp"
#include "curloptions.hpp"
#include "currencycode.hpp"
#include "file.hpp"
#include "httprequesttype.hpp"
#include "permanentcurloptions.hpp"
#include "timedef.hpp"
#include "withdrawalfees-crawler.hpp"

namespace cct::api {
namespace {

File GetFiatCacheFile(std::string_view dataDir) {
  return {dataDir, File::Type::kCache, "fiatcache.json", File::IfError::kNoThrow};
}

}  // namespace

CommonAPI::CommonAPI(const CoincenterInfo& coincenterInfo, Duration fiatsUpdateFrequency,
                     Duration withdrawalFeesUpdateFrequency, AtInit atInit)
    : _coincenterInfo(coincenterInfo),
      _fiatsCache(CachedResultOptions(fiatsUpdateFrequency, _cachedResultVault)),
      _withdrawalFeesCrawler(coincenterInfo, withdrawalFeesUpdateFrequency, _cachedResultVault) {
  if (atInit == AtInit::kLoadFromFileCache) {
    json data = GetFiatCacheFile(_coincenterInfo.dataDir()).readAllJson();
    if (!data.empty()) {
      int64_t timeEpoch = data["timeepoch"].get<int64_t>();
      auto& fiatsFile = data["fiats"];
      Fiats fiats;
      fiats.reserve(static_cast<Fiats::size_type>(fiatsFile.size()));
      for (json& val : fiatsFile) {
        log::trace("Reading fiat {} from cache file", val.get<std::string_view>());
        fiats.emplace_hint(fiats.end(), std::move(val.get_ref<string&>()));
      }
      log::debug("Loaded {} fiats from cache file", fiats.size());
      _fiatsCache.set(std::move(fiats), TimePoint(seconds(timeEpoch)));
    }
  }
}

CommonAPI::Fiats CommonAPI::queryFiats() {
  std::lock_guard<std::mutex> guard(_globalMutex);
  return _fiatsCache.get();
}

bool CommonAPI::queryIsCurrencyCodeFiat(CurrencyCode currencyCode) { return queryFiats().contains(currencyCode); }

WithdrawalFeesCrawler::WithdrawalInfoMaps CommonAPI::queryWithdrawalFees(std::string_view exchangeName) {
  std::lock_guard<std::mutex> guard(_globalMutex);
  return _withdrawalFeesCrawler.get(exchangeName);
}

namespace {
constexpr std::string_view kFiatsUrlSource1 = "https://datahub.io/core/currency-codes/r/codes-all.json";
constexpr std::string_view kFiatsUrlSource2 = "https://www.iban.com/currency-codes";
}  // namespace

CommonAPI::FiatsFunc::FiatsFunc()
    : _curlHandle1(kFiatsUrlSource1, nullptr,
                   PermanentCurlOptions::Builder()
                       .setFollowLocation()
                       .setTooManyErrorsPolicy(PermanentCurlOptions::TooManyErrorsPolicy::kReturnEmptyResponse)
                       .build()),
      _curlHandle2(kFiatsUrlSource2, nullptr,
                   PermanentCurlOptions::Builder()
                       .setTooManyErrorsPolicy(PermanentCurlOptions::TooManyErrorsPolicy::kReturnEmptyResponse)
                       .build()) {}

CommonAPI::Fiats CommonAPI::FiatsFunc::operator()() {
  vector<CurrencyCode> fiatsVec = retrieveFiatsSource1();
  Fiats fiats;
  if (!fiatsVec.empty()) {
    fiats = Fiats(std::move(fiatsVec));
    log::info("Stored {} fiats from first source", fiats.size());
  } else {
    fiats = Fiats(retrieveFiatsSource2());
    log::info("Stored {} fiats from second source", fiats.size());
  }
  return fiats;
}

vector<CurrencyCode> CommonAPI::FiatsFunc::retrieveFiatsSource1() {
  vector<CurrencyCode> fiatsVec;

  std::string_view data = _curlHandle1.query("", CurlOptions(HttpRequestType::kGet));
  if (data.empty()) {
    log::error("Error parsing currency codes, no fiats found from first source");
    return fiatsVec;
  }
  static constexpr bool kAllowExceptions = false;
  json dataCSV = json::parse(data, nullptr, kAllowExceptions);
  if (dataCSV.is_discarded()) {
    log::error("Error parsing json data of currency codes from source 1");
    return fiatsVec;
  }
  for (const json& fiatData : dataCSV) {
    static constexpr std::string_view kCodeKey = "AlphabeticCode";
    static constexpr std::string_view kWithdrawalDateKey = "WithdrawalDate";

    auto codeIt = fiatData.find(kCodeKey);
    auto withdrawalDateIt = fiatData.find(kWithdrawalDateKey);
    if (codeIt != fiatData.end() && !codeIt->is_null() && withdrawalDateIt != fiatData.end() &&
        withdrawalDateIt->is_null()) {
      fiatsVec.emplace_back(codeIt->get<std::string_view>());
      log::debug("Stored {} fiat", codeIt->get<std::string_view>());
    }
  }

  return fiatsVec;
}

vector<CurrencyCode> CommonAPI::FiatsFunc::retrieveFiatsSource2() {
  vector<CurrencyCode> fiatsVec;
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
  auto data = fiatsCacheFile.readAllJson();
  const auto fiatsPtrLastUpdatedTimePair = _fiatsCache.retrieve();
  const auto timeEpochIt = data.find("timeepoch");
  if (timeEpochIt != data.end()) {
    const int64_t lastTimeFileUpdated = timeEpochIt->get<int64_t>();
    if (TimePoint(seconds(lastTimeFileUpdated)) >= fiatsPtrLastUpdatedTimePair.second) {
      return;  // No update
    }
  }
  data.clear();
  if (fiatsPtrLastUpdatedTimePair.first != nullptr) {
    for (CurrencyCode fiatCode : *fiatsPtrLastUpdatedTimePair.first) {
      data["fiats"].emplace_back(fiatCode.str());
    }
    data["timeepoch"] = TimestampToSecondsSinceEpoch(fiatsPtrLastUpdatedTimePair.second);
    fiatsCacheFile.write(data);
  }

  _withdrawalFeesCrawler.updateCacheFile();
}
}  // namespace cct::api
