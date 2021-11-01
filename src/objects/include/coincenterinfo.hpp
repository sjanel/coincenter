#pragma once

#include <assert.h>

#include <chrono>
#include <map>
#include <optional>
#include <string_view>
#include <unordered_map>

#include "apiquerytypeenum.hpp"
#include "cct_const.hpp"
#include "cct_run_modes.hpp"
#include "cct_string.hpp"
#include "currencycode.hpp"
#include "exchangeinfo.hpp"

namespace cct {
class CoincenterInfo {
 public:
  using CurrencyEquivalentAcronymMap = std::unordered_map<CurrencyCode, CurrencyCode>;
  using ExchangeInfoMap = std::map<string, ExchangeInfo, std::less<>>;
  using Duration = std::chrono::high_resolution_clock::duration;
  using StableCoinsMap = std::unordered_map<CurrencyCode, CurrencyCode>;

  explicit CoincenterInfo(settings::RunMode runMode = settings::RunMode::kProd,
                          std::string_view dataDir = kDefaultDataDir);

  CoincenterInfo(const CoincenterInfo &) = delete;
  CoincenterInfo &operator=(const CoincenterInfo &) = delete;

  CoincenterInfo(CoincenterInfo &&) = default;
  CoincenterInfo &operator=(CoincenterInfo &&) = default;

  /// Sometimes, XBT is used instead of BTC for Bitcoin.
  /// Use this function to standardize names (will return BTC for the latter)
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

  Duration getAPICallUpdateFrequency(api::QueryTypeEnum apiCallType) const {
    auto it = _apiCallUpdateFrequencyMap.find(apiCallType);
    assert(it != _apiCallUpdateFrequencyMap.end());
    return it->second;
  }

  settings::RunMode getRunMode() const { return _runMode; }

  std::string_view dataDir() const { return _dataDir; }

  bool useMonitoring() const { return _useMonitoring; }

 private:
  using APICallUpdateFrequencyMap = std::unordered_map<api::QueryTypeEnum, Duration>;

  CurrencyEquivalentAcronymMap _currencyEquiAcronymMap;
  StableCoinsMap _stableCoinsMap;
  APICallUpdateFrequencyMap _apiCallUpdateFrequencyMap;
  ExchangeInfoMap _exchangeInfoMap;
  settings::RunMode _runMode;
  string _dataDir;
  bool _useMonitoring;
};
}  // namespace cct
