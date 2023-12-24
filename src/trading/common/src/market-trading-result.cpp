#include "market-trading-result.hpp"

#include <string_view>
#include <utility>

#include "exchangeprivateapitypes.hpp"
#include "monetaryamount.hpp"

namespace cct {

MarketTradingResult::MarketTradingResult(std::string_view algorithmName, MonetaryAmount startBaseAmount,
                                         MonetaryAmount startQuoteAmount, MonetaryAmount quoteAmountDelta,
                                         ClosedOrderVector matchedOrders)
    : _algorithmName(algorithmName),
      _startBaseAmount(startBaseAmount),
      _startQuoteAmount(startQuoteAmount),
      _quoteAmountDelta(quoteAmountDelta),
      _matchedOrders(std::move(matchedOrders)) {}

}  // namespace cct