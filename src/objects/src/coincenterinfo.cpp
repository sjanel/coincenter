#include "coincenterinfo.hpp"

#include "cct_const.hpp"
#include "cct_exception.hpp"
#include "cct_file.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"

#ifdef CCT_ENABLE_PROMETHEUS
#include "prometheusmetricgateway.hpp"
#else
#include "voidmetricgateway.hpp"
#endif

namespace cct {

namespace {
CoincenterInfo::CurrencyEquivalentAcronymMap ComputeCurrencyEquivalentAcronymMap(std::string_view dataDir) {
  File currencyAcronymsTranslatorFile(dataDir, File::Type::kStatic, "currencyacronymtranslator.json",
                                      File::IfNotFound::kThrow);
  json jsonData = currencyAcronymsTranslatorFile.readJson();
  CoincenterInfo::CurrencyEquivalentAcronymMap map;
  for (const auto& [key, value] : jsonData.items()) {
    log::trace("Currency {} <=> {}", key, value.get<std::string_view>());
    map.insert_or_assign(CurrencyCode(key), value.get<std::string_view>());
  }
  return map;
}

CoincenterInfo::StableCoinsMap ComputeStableCoinsMap(std::string_view dataDir) {
  File stableCoinsFile(dataDir, File::Type::kStatic, "stablecoins.json", File::IfNotFound::kThrow);
  json jsonData = stableCoinsFile.readJson();
  CoincenterInfo::StableCoinsMap ret;
  for (const auto& [key, value] : jsonData.items()) {
    log::trace("Stable Crypto {} <=> {}", key, value.get<std::string_view>());
    ret.insert_or_assign(CurrencyCode(key), value.get<std::string_view>());
  }
  return ret;
}

#ifdef CCT_ENABLE_PROMETHEUS
using MetricGatewayType = PrometheusMetricGateway;
#else
using MetricGatewayType = VoidMetricGateway;
#endif

}  // namespace

CoincenterInfo::CoincenterInfo(settings::RunMode runMode, std::string_view dataDir,
                               const MonitoringInfo& monitoringInfo, bool printQueryResults)
    : _currencyEquiAcronymMap(ComputeCurrencyEquivalentAcronymMap(dataDir)),
      _stableCoinsMap(ComputeStableCoinsMap(dataDir)),
      // TODO: make below values configurable, with default value in a json file
      _apiCallUpdateFrequencyMap{{api::QueryTypeEnum::kCurrencies, std::chrono::hours(8)},
                                 {api::QueryTypeEnum::kMarkets, std::chrono::hours(8)},
                                 {api::QueryTypeEnum::kWithdrawalFees, std::chrono::hours(96)},
                                 {api::QueryTypeEnum::kAllOrderBooks, std::chrono::seconds(3)},
                                 {api::QueryTypeEnum::kOrderBook, std::chrono::seconds(1)},
                                 {api::QueryTypeEnum::kTradedVolume, std::chrono::hours(1)},
                                 {api::QueryTypeEnum::kLastPrice, std::chrono::seconds(1)},
                                 {api::QueryTypeEnum::kDepositWallet, std::chrono::hours(12)},
                                 {api::QueryTypeEnum::kNbDecimalsUnitsBithumb, std::chrono::hours(96)}},
      _exchangeInfoMap(ComputeExchangeInfoMap(dataDir)),
      _runMode(runMode),
      _dataDir(dataDir),
      _metricGatewayPtr(_runMode == settings::RunMode::kProd && monitoringInfo.useMonitoring()
                            ? new MetricGatewayType(monitoringInfo)
                            : nullptr),
      _monitoringInfo(monitoringInfo),
      _printQueryResults(printQueryResults) {}

CoincenterInfo::~CoincenterInfo() {}  // To have definition of MetricGateway

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