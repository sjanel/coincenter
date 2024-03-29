#pragma once

#include <cstdint>

namespace cct {
enum class TradeTimeoutAction : int8_t {
  kDefault,  // Use exchange config file default settings
  kCancel,   // When timeout of trade is reached, cancel remaining order
  kMatch     // When timeout of trade is reached, update remaining order at market price to force match
};

enum class TradeMode : int8_t {
  kSimulation,  // No real trade will be made. Useful for tests.
  kReal         // Real trade that will be executed in the exchange
};

/// Determines the default trade type if no override is present in the command.
/// A 'Single' trade is a trade from a start amount to a destination currency, on an exchange proposing the direct
/// conversion market.
/// A 'Multi' trade gives additional trading possibilities - in case the direct market from base to
/// target currency does not exist, engine evaluates the markets path reaching destination currency and apply the trades
/// in series.
// Example:
//  - Convert XRP to XLM on an exchange only proposing XRP-BTC and BTC-XLM markets.
enum class TradeTypePolicy : int8_t {
  kDefault,          // Use exchange config file default settings
  kForceMultiTrade,  // Force multi trade possibility
  kForceSingleTrade  // Force single trade only
};

enum class TradeSyncPolicy : int8_t {
  kSynchronous,  // Follow lifetime of the placed order, manage the price updates until it is either matched or
                 // cancelled.
  kAsynchronous  // Placed order will not be followed-up - trade will exits once placed.
};

}  // namespace cct