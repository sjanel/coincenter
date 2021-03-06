#pragma once

#include <cassert>
#include <memory>
#include <optional>
#include <string_view>
#include <unordered_map>

#include "apioutputtype.hpp"
#include "cct_string.hpp"
#include "currencycode.hpp"
#include "exchangeinfo.hpp"
#include "exchangeinfomap.hpp"
#include "generalconfig.hpp"
#include "loadconfiguration.hpp"
#include "monitoringinfo.hpp"
#include "runmodes.hpp"

namespace cct {

class AbstractMetricGateway;

class CoincenterInfo {
 public:
  using CurrencyEquivalentAcronymMap = std::unordered_map<CurrencyCode, CurrencyCode>;
  using StableCoinsMap = std::unordered_map<CurrencyCode, CurrencyCode>;

  explicit CoincenterInfo(settings::RunMode runMode = settings::RunMode::kProd,
                          const LoadConfiguration &loadConfiguration = LoadConfiguration(),
                          GeneralConfig &&generalConfig = GeneralConfig(),
                          MonitoringInfo &&monitoringInfo = MonitoringInfo());

  ~CoincenterInfo();

  /// Sometimes, XBT is used instead of BTC for Bitcoin.
  /// Use this function to standardize names
  CurrencyCode standardizeCurrencyCode(CurrencyCode currencyCode) const;

  CurrencyCode standardizeCurrencyCode(std::string_view currencyCode) const {
    return standardizeCurrencyCode(CurrencyCode(currencyCode));
  }

  /// If 'stableCoinCandidate' is a stable crypto currency, return its associated fiat currency code.
  /// Otherwise, return 'std::nullopt'
  std::optional<CurrencyCode> fiatCurrencyIfStableCoin(CurrencyCode stableCoinCandidate) const;

  const ExchangeInfo &exchangeInfo(std::string_view exchangeName) const {
    auto it = _exchangeInfoMap.find(exchangeName);
    assert(it != _exchangeInfoMap.end() && "Unable to find this exchange in the configuration file");
    return it->second;
  }

  settings::RunMode getRunMode() const { return _runMode; }

  std::string_view dataDir() const { return _dataDir; }

  bool useMonitoring() const { return _monitoringInfo.useMonitoring(); }

  AbstractMetricGateway &metricGateway() const {
    assert(useMonitoring());
    return *_metricGatewayPtr;
  }

  AbstractMetricGateway *metricGatewayPtr() const { return _metricGatewayPtr.get(); }

  ApiOutputType apiOutputType() const { return _generalConfig.apiOutputType(); }

  Duration fiatConversionQueryRate() const { return _generalConfig.fiatConversionQueryRate(); }

 private:
  CurrencyEquivalentAcronymMap _currencyEquiAcronymMap;
  StableCoinsMap _stableCoinsMap;
  ExchangeInfoMap _exchangeInfoMap;
  settings::RunMode _runMode;
  string _dataDir;
  GeneralConfig _generalConfig;
  std::unique_ptr<AbstractMetricGateway> _metricGatewayPtr;
  MonitoringInfo _monitoringInfo;
};
}  // namespace cct
