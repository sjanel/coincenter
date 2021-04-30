#pragma once

#include <assert.h>

#include <chrono>
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
  using CurrencyEquivalentAcronymMap = std::unordered_map<std::string, std::string>;
  using ExchangeInfoMap = std::unordered_map<std::string, ExchangeInfo>;
  using Duration = std::chrono::high_resolution_clock::duration;

  CoincenterInfo(settings::RunMode runMode = settings::kProd);

  CoincenterInfo(const CoincenterInfo &) = delete;
  CoincenterInfo &operator=(const CoincenterInfo &) = delete;

  CoincenterInfo(CoincenterInfo &&) = default;
  CoincenterInfo &operator=(CoincenterInfo &&) = default;

  /// Sometimes, XBT is used instead of BTC for Bitcoin.
  /// Use this function to standardize names (will return BTC for the latter)
  std::string_view standardizeCurrencyCode(std::string_view currencyCode) const;

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

  const CurrencyEquivalentAcronymMap _currencyEquiAcronymMap;
  const APICallUpdateFrequencyMap _apiCallUpdateFrequencyMap;
  const ExchangeInfoMap _exchangeInfoMap;
  const settings::RunMode _runMode;
  const bool _useMonitoring;
};
}  // namespace cct
