#pragma once

#include <span>
#include <string_view>

#include "cct_type_traits.hpp"
#include "closed-order.hpp"
#include "exchangeprivateapitypes.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"

namespace cct {

class MarketTradingResult {
 public:
  MarketTradingResult() noexcept = default;

  MarketTradingResult(std::string_view algorithmName, MonetaryAmount startBaseAmount, MonetaryAmount startQuoteAmount,
                      MonetaryAmount quoteAmountDelta, ClosedOrderVector matchedOrders);

  std::string_view algorithmName() const { return _algorithmName; }

  Market market() const { return {_startBaseAmount.currencyCode(), _startQuoteAmount.currencyCode()}; }

  MonetaryAmount startBaseAmount() const { return _startBaseAmount; }

  MonetaryAmount startQuoteAmount() const { return _startQuoteAmount; }

  MonetaryAmount quoteAmountDelta() const { return _quoteAmountDelta; }

  std::span<const ClosedOrder> matchedOrders() const { return _matchedOrders; }

  using trivially_relocatable = is_trivially_relocatable<ClosedOrderVector>::type;

 private:
  std::string_view _algorithmName;
  MonetaryAmount _startBaseAmount;
  MonetaryAmount _startQuoteAmount;
  MonetaryAmount _quoteAmountDelta;
  ClosedOrderVector _matchedOrders;
};

}  // namespace cct