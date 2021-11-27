#include "cryptowatchapi.hpp"

#include <cctype>

#include "cct_exception.hpp"
#include "cct_file.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "coincenterinfo.hpp"
#include "toupperlower.hpp"

namespace cct {
namespace api {
namespace {
constexpr std::string_view kUrlBase = "https://api.cryptowat.ch";
constexpr char kUserAgent[] = "Cryptowatch C++ API Client";

string Query(CurlHandle& curlHandle, std::string_view method, CurlPostData&& postData = CurlPostData()) {
  string method_url(kUrlBase);
  method_url.push_back('/');
  method_url.append(method);

  CurlOptions opts(CurlOptions::RequestType::kGet);
  opts.userAgent = kUserAgent;
  opts.postdata = std::move(postData);

  return curlHandle.query(method_url, opts);
}

const json& CollectResults(const json& dataJson) {
  auto errIt = dataJson.find("error");
  if (errIt != dataJson.end() && !errIt->empty()) {
    std::string_view errMsg = errIt->front().get<std::string_view>();
    string ex("Cryptowatch::query error: ");
    ex.append(errMsg);
    throw exception(std::move(ex));
  }
  return dataJson["result"];
}

File GetFiatCacheFile(std::string_view dataDir) {
  return File(dataDir, File::Type::kCache, "fiatcache.json", File::IfNotFound::kNoThrow);
}
}  // namespace

CryptowatchAPI::CryptowatchAPI(const CoincenterInfo& config, settings::RunMode runMode,
                               Clock::duration fiatsUpdateFrequency, bool loadFromFileCacheAtInit)
    : _config(config),
      _curlHandle(config.metricGatewayPtr(), Clock::duration::zero(), runMode),
      _fiatsCache(CachedResultOptions(fiatsUpdateFrequency, _cachedResultVault), _curlHandle),
      _supportedExchanges(CachedResultOptions(std::chrono::hours(96), _cachedResultVault), _curlHandle),
      _allPricesCache(CachedResultOptions(std::chrono::seconds(30), _cachedResultVault), _curlHandle) {
  if (loadFromFileCacheAtInit) {
    json data = GetFiatCacheFile(_config.dataDir()).readJson();
    if (!data.empty()) {
      const int64_t timeepoch = data["timeepoch"].get<int64_t>();
      const auto& fiatsFile = data["fiats"];
      Fiats fiats;
      fiats.reserve(static_cast<Fiats::size_type>(fiatsFile.size()));
      for (auto it = fiatsFile.begin(), endIt = fiatsFile.end(); it != endIt; ++it) {
        log::debug("Reading fiat {} from cache file", it->get<std::string_view>());
        fiats.emplace(it->get<std::string_view>());
      }
      log::info("Loaded {} fiats from cache file", fiats.size());
      _fiatsCache.set(std::move(fiats), TimePoint(std::chrono::seconds(timeepoch)));
    }
  }
}

std::optional<double> CryptowatchAPI::queryPrice(std::string_view exchangeName, Market m) {
  string marketPrefix("market:");
  marketPrefix.append(exchangeName);
  marketPrefix.push_back(':');

  std::lock_guard<std::mutex> guard(_pricesMutex);

  for (int marketPos = 0; marketPos < 2; ++marketPos) {
    string lowerStrMarket = m.assetsPairStr();
    std::transform(lowerStrMarket.begin(), lowerStrMarket.end(), lowerStrMarket.begin(),
                   [](char c) { return tolower(c); });
    string mStr = marketPrefix;
    mStr.append(lowerStrMarket);

    const json& result = _allPricesCache.get();
    auto foundIt = result.find(mStr);
    if (foundIt != result.end()) {
      const double p = static_cast<double>(*foundIt);
      return marketPos == 0 ? p : (1 / p);
    }
    m = m.reverse();  // Second try with reversed market
  }
  return std::nullopt;
}

CryptowatchAPI::Fiats CryptowatchAPI::FiatsFunc::operator()() {
  json dataJson = json::parse(Query(_curlHandle, "assets"));
  const json& result = CollectResults(dataJson);
  Fiats fiats;
  for (const json& assetDetails : result) {
    auto foundIt = assetDetails.find("fiat");
    if (foundIt != assetDetails.end() && foundIt->get<bool>()) {
      CurrencyCode fiatCode(assetDetails["symbol"].get<std::string_view>());
      log::debug("Storing fiat {}", fiatCode.str());
      fiats.insert(std::move(fiatCode));
    }
  }
  log::info("Stored {} fiats", fiats.size());
  return fiats;
}

CryptowatchAPI::SupportedExchanges CryptowatchAPI::SupportedExchangesFunc::operator()() {
  SupportedExchanges ret;
  json dataJson = json::parse(Query(_curlHandle, "exchanges"));
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
  json dataJson = json::parse(Query(_curlHandle, "markets/prices"));
  return CollectResults(dataJson);
}

void CryptowatchAPI::updateCacheFile() const {
  File fiatsCacheFile = GetFiatCacheFile(_config.dataDir());
  json data = fiatsCacheFile.readJson();
  auto fiatsPtrLastUpdatedTimePair = _fiatsCache.retrieve();
  auto timeEpochIt = data.find("timeepoch");
  if (timeEpochIt != data.end()) {
    int64_t lastTimeFileUpdated = timeEpochIt->get<int64_t>();
    if (TimePoint(std::chrono::seconds(lastTimeFileUpdated)) >= fiatsPtrLastUpdatedTimePair.second) {
      return;  // No update
    }
  }
  data.clear();
  if (fiatsPtrLastUpdatedTimePair.first) {
    for (CurrencyCode fiatCode : *fiatsPtrLastUpdatedTimePair.first) {
      data["fiats"].emplace_back(fiatCode.str());
    }
    data["timeepoch"] =
        std::chrono::duration_cast<std::chrono::seconds>(fiatsPtrLastUpdatedTimePair.second.time_since_epoch()).count();
    fiatsCacheFile.write(data);
  }
}
}  // namespace api
}  // namespace cct