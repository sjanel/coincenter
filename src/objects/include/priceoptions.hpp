#pragma once

#include <limits>
#include <string_view>

#include "cct_string.hpp"
#include "exchange-query-config.hpp"
#include "monetaryamount.hpp"
#include "priceoptionsdef.hpp"

namespace cct {

class PriceOptions {
 public:
  /// Constructs a PriceOptions with a maker strategy
  constexpr PriceOptions() noexcept = default;

  /// Constructs a PriceOptions with a designated strategy
  constexpr explicit PriceOptions(PriceStrategy tradeStrategy) noexcept
      : _priceStrategy(tradeStrategy), _isDefault(false) {}

  /// Constructs a PriceOptions based on a continuously updated price from given string representation of trade
  /// strategy
  explicit PriceOptions(std::string_view priceStrategyStr);

  /// Constructs a PriceOptions based on a fixed absolute price.
  /// Multi trade is not supported in this case.
  explicit PriceOptions(MonetaryAmount fixedPrice);

  /// Constructs a PriceOptions based on a fixed relative price (relative from limit price).
  explicit PriceOptions(RelativePrice relativePrice);

  /// Constructs a PriceOptions based on given trade configuration
  explicit PriceOptions(const schema::ExchangeQueryTradeConfig &tradeConfig);

  constexpr PriceStrategy priceStrategy() const { return _priceStrategy; }

  constexpr MonetaryAmount fixedPrice() const { return _fixedPrice; }

  constexpr int relativePrice() const { return _relativePrice; }

  constexpr bool isTakerStrategy() const { return _priceStrategy == PriceStrategy::taker; }

  constexpr bool isFixedPrice() const { return !_fixedPrice.isDefault(); }

  constexpr bool isRelativePrice() const { return _relativePrice != kNoRelativePrice; }

  constexpr bool isAveragePrice() const {
    return !isFixedPrice() && !isTakerStrategy() && (!isRelativePrice() || relativePrice() == 0);
  }

  constexpr void switchToTakerStrategy() { _priceStrategy = PriceStrategy::taker; }

  std::string_view priceStrategyStr(bool placeRealOrderInSimulationMode) const;

  string str(bool placeRealOrderInSimulationMode) const;

  bool isDefault() const { return _isDefault; }

  bool operator==(const PriceOptions &) const noexcept = default;

 private:
  static constexpr RelativePrice kNoRelativePrice = std::numeric_limits<RelativePrice>::min();

  MonetaryAmount _fixedPrice;
  RelativePrice _relativePrice = kNoRelativePrice;
  PriceStrategy _priceStrategy = PriceStrategy::maker;
  bool _isDefault = true;  // to know if exchanges can use exchange config settings
};
}  // namespace cct