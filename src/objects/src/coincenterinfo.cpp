#include "coincenterinfo.hpp"

#include "cct_const.hpp"
#include "cct_exception.hpp"
#include "cct_file.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "durationstring.hpp"
#include "exchangeinfoparser.hpp"

#ifdef CCT_ENABLE_PROMETHEUS
#include "prometheusmetricgateway.hpp"
#else
#include "voidmetricgateway.hpp"
#endif

namespace cct {

namespace {
CoincenterInfo::CurrencyEquivalentAcronymMap ComputeCurrencyEquivalentAcronymMap(std::string_view dataDir) {
  File currencyAcronymsTranslatorFile(dataDir, File::Type::kStatic, "currencyacronymtranslator.json",
                                      File::IfError::kThrow);
  json jsonData = currencyAcronymsTranslatorFile.readJson();
  CoincenterInfo::CurrencyEquivalentAcronymMap map;
  for (const auto& [key, value] : jsonData.items()) {
    log::trace("Currency {} <=> {}", key, value.get<std::string_view>());
    map.insert_or_assign(CurrencyCode(key), value.get<std::string_view>());
  }
  return map;
}

CoincenterInfo::StableCoinsMap ComputeStableCoinsMap(std::string_view dataDir) {
  File stableCoinsFile(dataDir, File::Type::kStatic, "stablecoins.json", File::IfError::kThrow);
  json jsonData = stableCoinsFile.readJson();
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
                               GeneralConfig&& generalConfig, MonitoringInfo&& monitoringInfo)
    : _currencyEquiAcronymMap(ComputeCurrencyEquivalentAcronymMap(loadConfiguration.dataDir())),
      _stableCoinsMap(ComputeStableCoinsMap(loadConfiguration.dataDir())),
      _exchangeInfoMap(ComputeExchangeInfoMap(LoadExchangeConfigData(loadConfiguration))),
      _runMode(runMode),
      _dataDir(loadConfiguration.dataDir()),
      _generalConfig(std::move(generalConfig)),
      _metricGatewayPtr(_runMode == settings::RunMode::kProd && monitoringInfo.useMonitoring()
                            ? new MetricGatewayType(monitoringInfo)
                            : nullptr),
      _monitoringInfo(monitoringInfo) {}

CoincenterInfo::~CoincenterInfo() = default;  // To have definition of MetricGateway

CurrencyCode CoincenterInfo::standardizeCurrencyCode(CurrencyCode currencyCode) const {
  auto it = _currencyEquiAcronymMap.find(currencyCode);
  if (it != _currencyEquiAcronymMap.end()) {
    return it->second;
  }
  return currencyCode;
}

std::optional<CurrencyCode> CoincenterInfo::fiatCurrencyIfStableCoin(CurrencyCode stableCoinCandidate) const {
  auto it = _stableCoinsMap.find(stableCoinCandidate);
  if (it != _stableCoinsMap.end()) {
    return it->second;
  }
  return std::nullopt;
}

}  // namespace cct