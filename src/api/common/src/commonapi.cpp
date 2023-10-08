#include "commonapi.hpp"

#include <cctype>
#include <string_view>

#include "cct_exception.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "coincenterinfo.hpp"
#include "curloptions.hpp"
#include "file.hpp"
#include "permanentcurloptions.hpp"
#include "timedef.hpp"

namespace cct::api {
namespace {

File GetFiatCacheFile(std::string_view dataDir) {
  return {dataDir, File::Type::kCache, "fiatcache.json", File::IfError::kNoThrow};
}

}  // namespace

CommonAPI::CommonAPI(const CoincenterInfo& config, Duration fiatsUpdateFrequency, bool loadFromFileCacheAtInit)
    : _coincenterInfo(config), _fiatsCache(CachedResultOptions(fiatsUpdateFrequency, _cachedResultVault)) {
  if (loadFromFileCacheAtInit) {
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
      _fiatsCache.set(std::move(fiats), TimePoint(std::chrono::seconds(timeEpoch)));
    }
  }
}

namespace {
constexpr std::string_view kFiatsUrl = "https://datahub.io/core/currency-codes/r/codes-all.json";
}

CommonAPI::FiatsFunc::FiatsFunc()
    : _curlHandle(kFiatsUrl, nullptr, PermanentCurlOptions::Builder().setFollowLocation().build()) {}

CommonAPI::Fiats CommonAPI::FiatsFunc::operator()() {
  json dataCSV = json::parse(_curlHandle.query("", CurlOptions(HttpRequestType::kGet)));
  Fiats fiats;
  for (const json& fiatData : dataCSV) {
    static constexpr std::string_view kCodeKey = "AlphabeticCode";
    static constexpr std::string_view kWithdrawalDateKey = "WithdrawalDate";
    auto codeIt = fiatData.find(kCodeKey);
    auto withdrawalDateIt = fiatData.find(kWithdrawalDateKey);
    if (codeIt != fiatData.end() && !codeIt->is_null() && withdrawalDateIt != fiatData.end() &&
        withdrawalDateIt->is_null()) {
      fiats.insert(CurrencyCode(codeIt->get<std::string_view>()));
      log::debug("Stored {} fiat", codeIt->get<std::string_view>());
    }
  }
  if (fiats.empty()) {
    throw exception("Error parsing currency codes, no fiats found in {}", dataCSV.dump());
  }

  log::info("Stored {} fiats", fiats.size());
  return fiats;
}

void CommonAPI::updateCacheFile() const {
  File fiatsCacheFile = GetFiatCacheFile(_coincenterInfo.dataDir());
  json data = fiatsCacheFile.readAllJson();
  auto fiatsPtrLastUpdatedTimePair = _fiatsCache.retrieve();
  auto timeEpochIt = data.find("timeepoch");
  if (timeEpochIt != data.end()) {
    int64_t lastTimeFileUpdated = timeEpochIt->get<int64_t>();
    if (TimePoint(std::chrono::seconds(lastTimeFileUpdated)) >= fiatsPtrLastUpdatedTimePair.second) {
      return;  // No update
    }
  }
  data.clear();
  if (fiatsPtrLastUpdatedTimePair.first != nullptr) {
    for (CurrencyCode fiatCode : *fiatsPtrLastUpdatedTimePair.first) {
      data["fiats"].emplace_back(fiatCode.str());
    }
    data["timeepoch"] = TimestampToS(fiatsPtrLastUpdatedTimePair.second);
    fiatsCacheFile.write(data);
  }
}
}  // namespace cct::api
