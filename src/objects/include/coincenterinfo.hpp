#pragma once

#include <assert.h>

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "apiquerytypeenum.hpp"
#include "cct_run_modes.hpp"
#include "currencycode.hpp"
#include "exchangeinfo.hpp"

namespace cct {
class CoincenterInfo {
 public:
  using CurrencyEquivalentAcronymMap = std::unordered_map<CurrencyCode, CurrencyCode>;
  using ExchangeInfoMap = std::unordered_map<std::string, ExchangeInfo>;
  using Duration = std::chrono::high_resolution_clock::duration;
  using StableCoinsMap = std::unordered_map<CurrencyCode, CurrencyCode>;

  explicit CoincenterInfo(settings::RunMode runMode = settings::RunMode::kProd);

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
    return _exchangeInfoMap.find(std::string(exchangeName))->second;
  }

  Duration getAPICallUpdateFrequency(api::QueryTypeEnum apiCallType) const {
    return _apiCallUpdateFrequencyMap.find(apiCallType)->second;
  }

  settings::RunMode getRunMode() const { return _runMode; }

  bool useMonitoring() const { return _useMonitoring; }

 private:
  using APICallUpdateFrequencyMap = std::unordered_map<api::QueryTypeEnum, Duration>;

  CurrencyEquivalentAcronymMap _currencyEquiAcronymMap;
  StableCoinsMap _stableCoinsMap;
  APICallUpdateFrequencyMap _apiCallUpdateFrequencyMap;
  ExchangeInfoMap _exchangeInfoMap;
  settings::RunMode _runMode;
  bool _useMonitoring;
};
}  // namespace cct
