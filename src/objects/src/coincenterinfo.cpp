#include "coincenterinfo.hpp"

#include <algorithm>
#include <iterator>
#include <string_view>
#include <utility>

#include "cct_exception.hpp"
#include "cct_log.hpp"
#include "cct_string.hpp"
#include "currencycode.hpp"
#include "general-config.hpp"
#include "loadconfiguration.hpp"
#include "logginginfo.hpp"
#include "monitoringinfo.hpp"
#include "read-json.hpp"
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

CoincenterInfo::CurrencyEquivalentAcronymMap ComputeCurrencyEquivalentAcronymMap(const Reader& reader) {
  CoincenterInfo::CurrencyEquivalentAcronymMap map;
  ReadJsonOrThrow(reader.readAll(), map);
  return map;
}

CoincenterInfo::StableCoinsMap ComputeStableCoinsMap(const Reader& reader) {
  CoincenterInfo::StableCoinsMap map;
  ReadJsonOrThrow(reader.readAll(), map);
  return map;
}

#ifdef CCT_ENABLE_PROMETHEUS
using MetricGatewayType = PrometheusMetricGateway;
#else
using MetricGatewayType = VoidMetricGateway;
#endif

}  // namespace

CoincenterInfo::CoincenterInfo(settings::RunMode runMode, const LoadConfiguration& loadConfiguration,
                               schema::GeneralConfig&& generalConfig, LoggingInfo&& loggingInfo,
                               MonitoringInfo&& monitoringInfo, const Reader& currencyAcronymsReader,
                               const Reader& stableCoinsReader, const Reader& currencyPrefixesReader)
    : _currencyEquiAcronymMap(ComputeCurrencyEquivalentAcronymMap(currencyAcronymsReader)),
      _stableCoinsMap(ComputeStableCoinsMap(stableCoinsReader)),
      _allExchangeConfigs(loadConfiguration),
      _runMode(runMode),
      _dataDir(loadConfiguration.dataDir()),
      _generalConfig(std::move(generalConfig)),
      _loggingInfo(std::move(loggingInfo)),
      _metricGatewayPtr(_runMode == settings::RunMode::kProd && monitoringInfo.useMonitoring()
                            ? new MetricGatewayType(monitoringInfo)
                            : nullptr),
      _monitoringInfo(std::move(monitoringInfo)) {
  ReadJsonOrThrow(currencyPrefixesReader.readAll(), _currencyPrefixAcronymMap);
  for (auto& [prefix, acronym_prefix] : _currencyPrefixAcronymMap) {
    log::trace("Currency prefix {} <=> {}", prefix, acronym_prefix);
    _minPrefixLen = std::min(_minPrefixLen, static_cast<int>(prefix.length()));
    _maxPrefixLen = std::max(_maxPrefixLen, static_cast<int>(prefix.length()));
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
    string prefix = ToUpper(currencyCode.substr(0, prefixLen));
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

AbstractMetricGateway& CoincenterInfo::metricGateway() const {
  if (!useMonitoring()) {
    throw exception("Unexpected monitoring setting");
  }
  return *_metricGatewayPtr;
}

}  // namespace cct