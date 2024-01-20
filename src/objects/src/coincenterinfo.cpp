#include "coincenterinfo.hpp"

#include <algorithm>
#include <iterator>
#include <optional>
#include <string_view>
#include <utility>

#include "cct_exception.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "cct_string.hpp"
#include "currencycode.hpp"
#include "exchangeconfig.hpp"
#include "exchangeconfigmap.hpp"
#include "exchangeconfigparser.hpp"
#include "generalconfig.hpp"
#include "loadconfiguration.hpp"
#include "monitoringinfo.hpp"
#include "reader.hpp"
#include "runmodes.hpp"
#include "toupperlower-string.hpp"
#include "toupperlower.hpp"
#ifdef CCT_ENABLE_PROMETHEUS
#include "prometheusmetricgateway.hpp"
#else
#include "voidmetricgateway.hpp"
#endif

namespace cct {

namespace {
CoincenterInfo::CurrencyEquivalentAcronymMap ComputeCurrencyEquivalentAcronymMap(
    const Reader& currencyAcronymsTranslatorReader) {
  json jsonData = currencyAcronymsTranslatorReader.readAllJson();
  CoincenterInfo::CurrencyEquivalentAcronymMap map;
  map.reserve(jsonData.size());
  for (const auto& [key, value] : jsonData.items()) {
    log::trace("Currency {} <=> {}", key, value.get<std::string_view>());
    map.insert_or_assign(CurrencyCode(key), value.get<std::string_view>());
  }
  return map;
}

CoincenterInfo::StableCoinsMap ComputeStableCoinsMap(const Reader& stableCoinsReader) {
  json jsonData = stableCoinsReader.readAllJson();
  CoincenterInfo::StableCoinsMap ret;
  for (const auto& [key, value] : jsonData.items()) {
    log::trace("Stable Crypto {} <=> {}", key, value.get<std::string_view>());
    ret.emplace(key, value.get<std::string_view>());
  }
  return ret;
}

#ifdef CCT_ENABLE_PROMETHEUS
using MetricGatewayType = PrometheusMetricGateway;
#else
using MetricGatewayType = VoidMetricGateway;
#endif

}  // namespace

CoincenterInfo::CoincenterInfo(settings::RunMode runMode, const LoadConfiguration& loadConfiguration,
                               GeneralConfig&& generalConfig, MonitoringInfo&& monitoringInfo,
                               const Reader& currencyAcronymsReader, const Reader& stableCoinsReader,
                               const Reader& currencyPrefixesReader)
    : _currencyEquiAcronymMap(ComputeCurrencyEquivalentAcronymMap(currencyAcronymsReader)),
      _stableCoinsMap(ComputeStableCoinsMap(stableCoinsReader)),
      _exchangeConfigMap(ComputeExchangeConfigMap(loadConfiguration.exchangeConfigFileName(),
                                                  LoadExchangeConfigData(loadConfiguration))),
      _runMode(runMode),
      _dataDir(loadConfiguration.dataDir()),
      _generalConfig(std::move(generalConfig)),
      _metricGatewayPtr(_runMode == settings::RunMode::kProd && monitoringInfo.useMonitoring()
                            ? new MetricGatewayType(monitoringInfo)
                            : nullptr),
      _monitoringInfo(std::move(monitoringInfo)) {
  json jsonData = currencyPrefixesReader.readAllJson();
  for (auto& [prefix, acronym_prefix] : jsonData.items()) {
    log::trace("Currency prefix {} <=> {}", prefix, acronym_prefix.get<std::string_view>());
    _minPrefixLen = std::min(_minPrefixLen, static_cast<int>(prefix.length()));
    _maxPrefixLen = std::max(_maxPrefixLen, static_cast<int>(prefix.length()));
    _currencyPrefixAcronymMap.insert_or_assign(ToUpper(prefix), std::move(acronym_prefix.get_ref<string&>()));
  }
}

CoincenterInfo::~CoincenterInfo() = default;  // To have definition of MetricGateway

CurrencyCode CoincenterInfo::standardizeCurrencyCode(CurrencyCode currencyCode) const {
  auto it = _currencyEquiAcronymMap.find(currencyCode);
  if (it != _currencyEquiAcronymMap.end()) {
    return it->second;
  }
  return currencyCode;
}

CurrencyCode CoincenterInfo::standardizeCurrencyCode(std::string_view currencyCode) const {
  auto maxPrefixLen = std::min(_maxPrefixLen, static_cast<int>(currencyCode.length()));
  string formattedCurrencyCode;
  for (int prefixLen = _minPrefixLen; prefixLen <= maxPrefixLen; ++prefixLen) {
    string prefix = ToUpper(std::string_view(currencyCode.begin(), currencyCode.begin() + prefixLen));
    auto lb = _currencyPrefixAcronymMap.lower_bound(prefix);
    if (lb == _currencyPrefixAcronymMap.end()) {
      // given currency code cannot have a prefix present in the map
      break;
    }
    if (lb->first == prefix) {
      formattedCurrencyCode.append(lb->second);
      auto begIt = currencyCode.begin() + prefixLen;
      auto endIt = currencyCode.end();
      auto isSeparator = [](char ch) { return ch == ' ' || ch == '/'; };

      while (begIt != endIt && isSeparator(*begIt)) {
        ++begIt;
      }
      std::transform(begIt, endIt, std::back_inserter(formattedCurrencyCode), toupper);
      log::debug("Transformed '{}' into '{}'", currencyCode, formattedCurrencyCode);
      currencyCode = formattedCurrencyCode;
      break;
    }
  }

  return standardizeCurrencyCode(CurrencyCode(currencyCode));
}

CurrencyCode CoincenterInfo::tryConvertStableCoinToFiat(CurrencyCode maybeStableCoin) const {
  const auto it = _stableCoinsMap.find(maybeStableCoin);
  if (it != _stableCoinsMap.end()) {
    return it->second;
  }
  return {};
}

const ExchangeConfig& CoincenterInfo::exchangeConfig(std::string_view exchangeName) const {
  auto it = _exchangeConfigMap.find(exchangeName);
  if (it == _exchangeConfigMap.end()) {
    throw exception("Unable to find this exchange in the configuration file");
  }
  return it->second;
}

AbstractMetricGateway& CoincenterInfo::metricGateway() const {
  if (!useMonitoring()) {
    throw exception("Unexpected monitoring setting");
  }
  return *_metricGatewayPtr;
}

}  // namespace cct