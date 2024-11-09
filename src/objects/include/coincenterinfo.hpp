#pragma once

#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <string_view>
#include <unordered_map>

#include "apioutputtype.hpp"
#include "cct_string.hpp"
#include "currencycode.hpp"
#include "exchange-config.hpp"
#include "general-config.hpp"
#include "loadconfiguration.hpp"
#include "logginginfo.hpp"
#include "monitoringinfo.hpp"
#include "reader.hpp"
#include "runmodes.hpp"
#include "timedef.hpp"

namespace cct {

class AbstractMetricGateway;

class CoincenterInfo {
 public:
  using CurrencyEquivalentAcronymMap = std::unordered_map<CurrencyCode, CurrencyCode>;
  using CurrencyPrefixAcronymMap = std::map<string, string, std::less<>>;
  using StableCoinsMap = std::unordered_map<CurrencyCode, CurrencyCode>;

  explicit CoincenterInfo(settings::RunMode runMode, const LoadConfiguration &loadConfiguration = LoadConfiguration(),
                          schema::GeneralConfig &&generalConfig = schema::GeneralConfig(),
                          LoggingInfo &&loggingInfo = LoggingInfo(), MonitoringInfo &&monitoringInfo = MonitoringInfo(),
                          const Reader &currencyAcronymsReader = Reader(), const Reader &stableCoinsReader = Reader(),
                          const Reader &currencyPrefixesReader = Reader());

  ~CoincenterInfo();

  /// Sometimes, XBT is used instead of BTC for Bitcoin.
  /// Use this method to standardize names
  CurrencyCode standardizeCurrencyCode(CurrencyCode currencyCode) const;
  CurrencyCode standardizeCurrencyCode(std::string_view currencyCode) const;
  CurrencyCode standardizeCurrencyCode(const char *currencyCode) const {
    return standardizeCurrencyCode(std::string_view(currencyCode));
  }

  /// If 'stableCoinCandidate' is a stable crypto currency, return its associated fiat currency code.
  /// Otherwise, return a default currency code
  CurrencyCode tryConvertStableCoinToFiat(CurrencyCode maybeStableCoin) const;

  const schema::ExchangeConfig &exchangeConfig(ExchangeNameEnum exchangeNameEnum) const {
    return _allExchangeConfigs[exchangeNameEnum];
  }

  settings::RunMode getRunMode() const { return _runMode; }

  std::string_view dataDir() const { return _dataDir; }

  bool useMonitoring() const { return _monitoringInfo.useMonitoring(); }

  AbstractMetricGateway &metricGateway() const;

  AbstractMetricGateway *metricGatewayPtr() const { return _metricGatewayPtr.get(); }

  const schema::GeneralConfig &generalConfig() const { return _generalConfig; }

  const LoggingInfo &loggingInfo() const { return _loggingInfo; }

  ApiOutputType apiOutputType() const { return _generalConfig.apiOutputType; }

  Duration fiatConversionQueryRate() const { return _generalConfig.fiatConversion.rate.duration; }

 private:
  CurrencyEquivalentAcronymMap _currencyEquiAcronymMap;
  CurrencyPrefixAcronymMap _currencyPrefixAcronymMap;
  StableCoinsMap _stableCoinsMap;
  schema::AllExchangeConfigs _allExchangeConfigs;
  settings::RunMode _runMode;
  string _dataDir;
  schema::GeneralConfig _generalConfig;
  LoggingInfo _loggingInfo;
  std::unique_ptr<AbstractMetricGateway> _metricGatewayPtr;
  MonitoringInfo _monitoringInfo;
  int _minPrefixLen = std::numeric_limits<int>::max();
  int _maxPrefixLen = 0;
};
}  // namespace cct
