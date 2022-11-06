#pragma once

#include <cstdint>

#include "currencycode.hpp"

namespace cct {
class BalanceOptions {
 public:
  enum class AmountIncludePolicy : int8_t {
    kOnlyAvailable,
    kWithBalanceInUse,
  };

  /// @brief Creates a default BalanceOptions.
  /// It has no equivalent currency to convert amounts to,
  /// and only available balance will be taken into account
  BalanceOptions() noexcept = default;

  /// @brief Constructs a BalanceOptions
  /// @param amountIncludePolicy  kOnlyAvailable: only available amounts will be returned
  ///                             kWithBalanceInUse: include balance in use by opened orders as well.
  /// @param equiCurrency for each amount in a currency, attempt to convert each asset to given currency as an
  ///                     additional value information. For instance: EUR, USD, BTC...
  BalanceOptions(AmountIncludePolicy amountIncludePolicy, CurrencyCode equiCurrency)
      : _equiCurrency(equiCurrency), _amountIncludePolicy(amountIncludePolicy) {}

  CurrencyCode equiCurrency() const { return _equiCurrency; }
  AmountIncludePolicy amountIncludePolicy() const { return _amountIncludePolicy; }

  constexpr bool operator==(const BalanceOptions &) const noexcept = default;

 private:
  CurrencyCode _equiCurrency;
  AmountIncludePolicy _amountIncludePolicy = AmountIncludePolicy::kOnlyAvailable;
};
}  // namespace cct