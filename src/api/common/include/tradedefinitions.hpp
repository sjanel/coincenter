#pragma once

#include <cstdint>

namespace cct {
enum class TradePriceStrategy : int8_t {
  kMaker,   // Place order at limit price.
  kNibble,  // Buy at 'limit + 1' price, sell at 'limit - 1' price (+-1 referring to previous or next price of the
            // orderbook). Benefits: you control the price, while at the same time speeding up the order execution
            // (compared to kMaker)
  kTaker    // Place order at market price for an expecting direct match
};

enum class TradeTimeoutAction : int8_t {
  kCancel,     // When timeout of trade is reached, cancel remaining order
  kForceMatch  // When timeout of trade is reached, update remaining order at market price to force match
};

enum class TradeMode : int8_t {
  kSimulation,  // No real trade will be made. Useful for tests.
  kReal         // Real trade that will be executed in the exchange
};

enum class TradeType : int8_t {
  kSingleTrade,  // Single, 'fast' trade from 'startAmount' into 'toCurrency', on exchange named 'privateExchangeName'.
                 // 'fast' means that no unnecessary checks are done prior to the trade query, but if trade is
                 // impossible exception will be thrown
  kMultiTradePossible  // A Multi trade is similar to a single trade, at the difference that it retrieves the fastest
                       // currency conversion path and will launch several 'single' trades to reach that final goal.
                       // Example:
                       //  - Convert XRP to XLM on an exchange only proposing XRP-BTC and BTC-XLM markets will make 2
                       //  trades on these
                       //    markets.
};
}  // namespace cct