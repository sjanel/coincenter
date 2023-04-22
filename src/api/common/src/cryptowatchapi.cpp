#include "cryptowatchapi.hpp"

#include <cctype>

#include "cct_exception.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "coincenterinfo.hpp"
#include "curloptions.hpp"
#include "file.hpp"
#include "timedef.hpp"
#include "toupperlower.hpp"

namespace cct::api {
namespace {
string Query(CurlHandle& curlHandle, std::string_view endpoint, CurlPostData&& postData = CurlPostData()) {
  return curlHandle.query(endpoint,
                          CurlOptions(HttpRequestType::kGet, std::move(postData), "Cryptowatch C++ API Client"));
}

const json& CollectResults(const json& dataJson) {
  auto errIt = dataJson.find("error");
  if (errIt != dataJson.end() && !errIt->empty()) {
    throw exception("Cryptowatch::query error: {}", errIt->front().get<std::string_view>());
  }
  return dataJson["result"];
}

File GetFiatCacheFile(std::string_view dataDir) {
  return {dataDir, File::Type::kCache, "fiatcache.json", File::IfError::kNoThrow};
}

constexpr std::string_view kCryptowatchBaseUrl = "https://api.cryptowat.ch";
}  // namespace

CryptowatchAPI::CryptowatchAPI(const CoincenterInfo& config, settings::RunMode runMode, Duration fiatsUpdateFrequency,
                               bool loadFromFileCacheAtInit)
    : _coincenterInfo(config),
      _curlHandle(kCryptowatchBaseUrl, config.metricGatewayPtr(), Duration::zero(), runMode),
      _fiatsCache(CachedResultOptions(fiatsUpdateFrequency, _cachedResultVault), _curlHandle),
      _supportedExchanges(CachedResultOptions(std::chrono::hours(96), _cachedResultVault), _curlHandle),
      _allPricesCache(CachedResultOptions(std::chrono::seconds(30), _cachedResultVault), _curlHandle) {
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

std::optional<double> CryptowatchAPI::queryPrice(std::string_view exchangeName, Market mk) {
  string marketPrefix("market:");
  marketPrefix.append(exchangeName);
  marketPrefix.push_back(':');

  std::lock_guard<std::mutex> guard(_pricesMutex);

  for (int marketPos = 0; marketPos < 2; ++marketPos) {
    string mStr = marketPrefix;
    mStr.append(mk.assetsPairStrLower());

    const json& result = _allPricesCache.get();
    auto foundIt = result.find(mStr);
    if (foundIt != result.end()) {
      const double price = static_cast<double>(*foundIt);
      return marketPos == 0 ? price : (1 / price);
    }
    mk = mk.reverse();  // Second try with reversed market
  }
  return std::nullopt;
}

CryptowatchAPI::Fiats CryptowatchAPI::FiatsFunc::operator()() {
  json dataJson = json::parse(Query(_curlHandle, "/assets"));
  const json& result = CollectResults(dataJson);
  Fiats fiats;
  for (const json& assetDetails : result) {
    auto foundIt = assetDetails.find("fiat");
    if (foundIt != assetDetails.end() && foundIt->get<bool>()) {
      CurrencyCode fiatCode(assetDetails["symbol"].get<std::string_view>());
      log::debug("Storing fiat {}", fiatCode);
      fiats.insert(std::move(fiatCode));
    }
  }
  log::info("Stored {} fiats", fiats.size());
  return fiats;
}

CryptowatchAPI::SupportedExchanges CryptowatchAPI::SupportedExchangesFunc::operator()() {
  SupportedExchanges ret;
  json dataJson = json::parse(Query(_curlHandle, "/exchanges"));
  const json& result = CollectResults(dataJson);
  for (const json& exchange : result) {
    std::string_view exchangeNameLowerCase = exchange["symbol"].get<std::string_view>();
    log::debug("{} is supported by Cryptowatch", exchangeNameLowerCase);
    ret.emplace(exchangeNameLowerCase);
  }
  log::info("{} exchanges supported by Cryptowatch", ret.size());
  return ret;
}

json CryptowatchAPI::AllPricesFunc::operator()() {
  json dataJson = json::parse(Query(_curlHandle, "/markets/prices"));
  return CollectResults(dataJson);
}

void CryptowatchAPI::updateCacheFile() const {
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
