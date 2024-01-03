#pragma once

#include <string_view>

#include "cct_string.hpp"
#include "monetaryamount.hpp"
#include "priceoptionsdef.hpp"

namespace cct {
class PriceOptions {
 public:
  /// Constructs a PriceOptions with a maker strategy
  constexpr PriceOptions() noexcept = default;

  /// Constructs a PriceOptions with a designated strategy
  constexpr explicit PriceOptions(PriceStrategy tradeStrategy) noexcept : _priceStrategy(tradeStrategy) {}

  /// Constructs a PriceOptions based on a continuously updated price from given string representation of trade
  /// strategy
  explicit PriceOptions(std::string_view priceStrategyStr);

  /// Constructs a PriceOptions based on a fixed absolute price.
  /// Multi trade is not supported in this case.
  explicit PriceOptions(MonetaryAmount fixedPrice) : _fixedPrice(fixedPrice) {}

  /// Constructs a PriceOptions based on a fixed relative price (relative from limit price).
  explicit PriceOptions(RelativePrice relativePrice);

  constexpr PriceStrategy priceStrategy() const { return _priceStrategy; }

  constexpr MonetaryAmount fixedPrice() const { return _fixedPrice; }

  constexpr int relativePrice() const { return _relativePrice; }

  constexpr bool isTakerStrategy() const { return _priceStrategy == PriceStrategy::kTaker; }

  constexpr bool isFixedPrice() const { return !_fixedPrice.isDefault(); }

  constexpr bool isRelativePrice() const { return _relativePrice != kNoRelativePrice; }

  constexpr bool isAveragePrice() const {
    return !isFixedPrice() && !isTakerStrategy() && (!isRelativePrice() || relativePrice() == 0);
  }

  constexpr void switchToTakerStrategy() { _priceStrategy = PriceStrategy::kTaker; }

  std::string_view priceStrategyStr(bool placeRealOrderInSimulationMode) const;

  string str(bool placeRealOrderInSimulationMode) const;

  bool operator==(const PriceOptions &) const noexcept = default;

 private:
  MonetaryAmount _fixedPrice;
  RelativePrice _relativePrice = kNoRelativePrice;
  PriceStrategy _priceStrategy = PriceStrategy::kMaker;
};
}  // namespace cct